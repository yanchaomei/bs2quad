#include "ImplicitCertificate.h"
#include <GeomAdaptor_Surface.hxx>
#include <gp_Pln.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Cone.hxx>
#include <gp_Sphere.hxx>
#include <gp_Torus.hxx>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <map>
#include <tuple>

namespace BS2Quad {
namespace {
// Every arithmetic operation is rounded outwards. Compile without -ffast-math:
// reassociation or contraction across these operations invalidates the enclosure.
struct Interval {
    double lo, hi;
    explicit Interval(double x = 0) : lo(x), hi(x) {
        if (!std::isfinite(x)) throw std::runtime_error("non-finite certificate input");
    }
    Interval(double a, double b) : lo(a), hi(b) {
        if (!std::isfinite(a) || !std::isfinite(b) || a>b)
            throw std::runtime_error("certificate arithmetic overflow");
    }
};
double Down(double x) { return std::nextafter(x,-std::numeric_limits<double>::infinity()); }
double Up(double x) { return std::nextafter(x,std::numeric_limits<double>::infinity()); }
bool Zero(Interval a) { return a.lo==0 && a.hi==0; }
Interval operator+(Interval a, Interval b) {
    if (Zero(a)) return b;
    if (Zero(b)) return a;
    return Interval(Down(a.lo+b.lo),Up(a.hi+b.hi));
}
Interval operator-(Interval a, Interval b) {
    if (Zero(b)) return a;
    if (a.lo==a.hi && b.lo==b.hi && a.lo==b.lo) return Interval(0);
    return Interval(Down(a.lo-b.hi),Up(a.hi-b.lo));
}
Interval operator*(Interval a, Interval b) {
    if (Zero(a)||Zero(b)) return Interval(0);
    // Bernstein weights are nonnegative. Their monotonic endpoint product
    // needs two products rather than four; outward rounding is unchanged.
    if(b.lo>=0) {
        if(a.lo>=0) return Interval(Down(a.lo*b.lo),Up(a.hi*b.hi));
        if(a.hi<=0) return Interval(Down(a.lo*b.hi),Up(a.hi*b.lo));
        return Interval(Down(a.lo*b.hi),Up(a.hi*b.hi));
    }
    const double values[]={a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
    return Interval(Down(*std::min_element(values,values+4)),
                    Up(*std::max_element(values,values+4)));
}
Interval operator/(Interval a, Interval b) {
    if (b.lo<=0 && b.hi>=0) throw std::runtime_error("zero certificate denominator");
    return a*Interval(Down(1/b.hi),Up(1/b.lo));
}
Interval SquareRoot(Interval a) {
    if (a.lo<0) throw std::runtime_error("negative certificate square root");
    return Interval(a.lo==0?0:Down(std::sqrt(a.lo)),Up(std::sqrt(a.hi)));
}
Interval Binomial(int n,int k) {
    Interval c(1);
    k=std::min(k,n-k);
    for (int i=1;i<=k;++i) c=c*Interval(n-k+i)/Interval(i);
    return c;
}
using HPoint=std::array<Interval,4>;
using Grid=std::vector<std::vector<HPoint>>;
HPoint Blend(const HPoint& a,const HPoint& b,Interval alpha) {
    HPoint p;
    for(int d=0;d<4;++d) p[d]=alpha*a[d]+(Interval(1)-alpha)*b[d];
    return p;
}

// One exact B-spline knot insertion in homogeneous coordinates, enclosed by
// interval arithmetic. Repeated to interior multiplicity p, each active span
// is a Bezier patch. We never replace source geometry by a fitted Bezier patch.
void InsertU(Grid& grid,std::vector<double>& knots,int degree,double t) {
    const int n=static_cast<int>(grid.size())-1;
    // At an unclamped active high endpoint, the existing knot is at n+1.
    // Forcing k=n would omit the final blend and can discard the last pole.
    // We only insert existing knots with multiplicity s<p, so k-s<=n here.
    const int k=static_cast<int>(std::upper_bound(knots.begin(),knots.end(),t)-knots.begin())-1;
    const int mult=static_cast<int>(std::count(knots.begin(),knots.end(),t));
    Grid next(grid.size()+1,std::vector<HPoint>(grid[0].size()));
    for(int i=0;i<=k-degree;++i) next[i]=grid[i];
    for(int i=k-mult;i<=n;++i) next[i+1]=grid[i];
    for(int i=k-degree+1;i<=k-mult;++i) {
        const Interval alpha=(Interval(t)-Interval(knots[i]))/
            (Interval(knots[i+degree])-Interval(knots[i]));
        for(std::size_t j=0;j<grid[0].size();++j)
            next[i][j]=Blend(grid[i][j],grid[i-1][j],alpha);
    }
    knots.insert(knots.begin()+k+1,t);
    grid.swap(next);
}
void Transpose(Grid& g) {
    Grid out(g[0].size(),std::vector<HPoint>(g.size()));
    for(std::size_t i=0;i<g.size();++i)
        for(std::size_t j=0;j<g[0].size();++j) out[j][i]=g[i][j];
    g.swap(out);
}
void BezierKnots(Grid& grid,std::vector<double>& knots,int degree) {
    const double low=knots[degree],high=knots[grid.size()];
    const auto old=knots;
    for(std::size_t i=0;i<old.size();) {
        const double t=old[i];
        const int m=static_cast<int>(std::count(old.begin(),old.end(),t));
        // Include active-domain endpoints of unclamped/periodic sequences.
        // Raising their multiplicities to p isolates the first/last Bezier
        // spans without altering geometry or assuming an approximated clamp.
        if(t>=low&&t<=high) for(int j=m;j<degree;++j) InsertU(grid,knots,degree,t);
        i+=static_cast<std::size_t>(m);
    }
}
std::vector<double> ExpandedKnots(const Handle(Geom_BSplineSurface)& s,bool u) {
    // OCCT's expanded knot sequence is the operational nonperiodic basis on
    // one active period, including exterior knots. Periodic virtual poles are
    // exact cyclic copies, not recomputed/dehomogenized floating coordinates.
    const auto& sequence=u?s->UKnotSequence():s->VKnotSequence();
    std::vector<double> knots;
    for(int i=sequence.Lower();i<=sequence.Upper();++i) {
        const double t=sequence(i);
        if(!std::isfinite(t)||(!knots.empty()&&t<knots.back()))
            throw std::runtime_error("invalid certificate knot sequence");
        knots.push_back(t);
    }
    return knots;
}

struct Polynomial {
    int u,v;
    std::vector<Interval> c;
    Polynomial(int p,int q) : u(p),v(q),c((p+1)*(q+1)) {}
    Interval& At(int i,int j) { return c[i*(v+1)+j]; }
    Interval At(int i,int j) const { return c[i*(v+1)+j]; }
};
Polynomial operator+(const Polynomial& a,const Polynomial& b) {
    if(a.u!=b.u||a.v!=b.v) throw std::runtime_error("certificate degree mismatch");
    Polynomial p(a.u,a.v);
    for(std::size_t i=0;i<p.c.size();++i) p.c[i]=a.c[i]+b.c[i];
    return p;
}
Polynomial operator-(const Polynomial& a,const Polynomial& b) {
    if(a.u!=b.u||a.v!=b.v) throw std::runtime_error("certificate degree mismatch");
    Polynomial p(a.u,a.v);
    for(std::size_t i=0;i<p.c.size();++i) p.c[i]=a.c[i]-b.c[i];
    return p;
}
Polynomial Scale(Polynomial a,Interval b) {
    for(auto& c:a.c) c=c*b;
    return a;
}
struct ProductTerm {
    std::size_t first,second,output;
    Interval weight;
};
const std::vector<ProductTerm>& ProductTerms(int au,int av,int bu,int bv,bool square) {
    // Bernstein product weights depend only on degrees. Cache their outward
    // enclosures once per thread; no geometric/input-dependent value is cached.
    // Squaring pairs (i,j),(k,l) has identical exact weights in either order,
    // so one off-diagonal contribution with weight 2 encloses both terms.
    using Key=std::tuple<int,int,int,int,bool>;
    static thread_local std::map<Key,std::vector<ProductTerm>> cache;
    const Key key{au,av,bu,bv,square};
    const auto found=cache.find(key);
    if(found!=cache.end()) return found->second;
    std::vector<std::vector<Interval>> u(au+1,std::vector<Interval>(bu+1));
    std::vector<std::vector<Interval>> v(av+1,std::vector<Interval>(bv+1));
    for(int i=0;i<=au;++i) for(int k=0;k<=bu;++k)
        u[i][k]=Binomial(au,i)*Binomial(bu,k)/Binomial(au+bu,i+k);
    for(int j=0;j<=av;++j) for(int l=0;l<=bv;++l)
        v[j][l]=Binomial(av,j)*Binomial(bv,l)/Binomial(av+bv,j+l);
    std::vector<ProductTerm> terms;
    const std::size_t na=(au+1)*(av+1),nb=(bu+1)*(bv+1);
    terms.reserve(square?na*(na+1)/2:na*nb);
    for(int i=0;i<=au;++i) for(int j=0;j<=av;++j)
        for(int k=0;k<=bu;++k) for(int l=0;l<=bv;++l) {
            const std::size_t first=i*(av+1)+j,second=k*(bv+1)+l;
            if(square&&second<first) continue;
            auto weight=u[i][k]*v[j][l];
            if(square&&first!=second) weight=weight*Interval(2);
            terms.push_back({first,second,static_cast<std::size_t>((i+k)*(av+bv+1)+j+l),weight});
        }
    return cache.emplace(key,std::move(terms)).first->second;
}
Polynomial Product(const Polynomial& a,const Polynomial& b,bool square) {
    Polynomial out(a.u+b.u,a.v+b.v);
    for(const auto& term:ProductTerms(a.u,a.v,b.u,b.v,square))
        out.c[term.output]=out.c[term.output]+a.c[term.first]*b.c[term.second]*term.weight;
    return out;
}
Polynomial operator*(const Polynomial& a,const Polynomial& b) {return Product(a,b,false);}
Polynomial Square(const Polynomial& a) {return Product(a,a,true);}

struct Analytic {
    GeomAbs_SurfaceType type;
    gp_Pnt origin;
    gp_Dir axis;
    double radius=0,major=0,angle=0;
};
Analytic Describe(const Handle(Geom_Surface)& surface) {
    GeomAdaptor_Surface a(surface);
    Analytic out;
    out.type=a.GetType();
    switch(out.type) {
    case GeomAbs_Plane: out.origin=a.Plane().Location(); out.axis=a.Plane().Axis().Direction(); break;
    case GeomAbs_Sphere: out.origin=a.Sphere().Location(); out.radius=a.Sphere().Radius(); break;
    case GeomAbs_Cylinder: out.origin=a.Cylinder().Location(); out.axis=a.Cylinder().Axis().Direction(); out.radius=a.Cylinder().Radius(); break;
    case GeomAbs_Cone: out.origin=a.Cone().Location(); out.axis=a.Cone().Axis().Direction(); out.radius=a.Cone().RefRadius(); out.angle=a.Cone().SemiAngle(); break;
    case GeomAbs_Torus: out.origin=a.Torus().Location(); out.axis=a.Torus().Axis().Direction(); out.radius=a.Torus().MinorRadius(); out.major=a.Torus().MajorRadius();
        if(!(out.major>out.radius&&out.radius>0)) throw std::runtime_error("only ring tori have this certificate");
        break;
    default: throw std::runtime_error("unsupported analytic certificate target");
    }
    return out;
}
std::pair<Interval,Interval> SineCosineEnclosures(double angle) {
    // Cone semi-angles are inside (-pi/2,pi/2). Interval Taylor arithmetic
    // plus an alternating-series remainder avoids depending on libm ulp
    // guarantees for the coefficients of the distance certificate.
    if(!std::isfinite(angle)||std::abs(angle)>2.0)
        throw std::runtime_error("unsupported cone semi-angle");
    const Interval x(angle),minusSquare=Interval(0)-x*x;
    Interval sine=x,cosine(1),sineTerm=x,cosineTerm(1);
    for(int n=1;n<=20;++n) {
        sineTerm=sineTerm*minusSquare/Interval((2*n)*(2*n+1));
        cosineTerm=cosineTerm*minusSquare/Interval((2*n-1)*(2*n));
        sine=sine+sineTerm; cosine=cosine+cosineTerm;
    }
    const Interval nextSine=sineTerm*minusSquare/Interval(42*43);
    const Interval nextCosine=cosineTerm*minusSquare/Interval(41*42);
    const double sineRemainder=std::max(std::abs(nextSine.lo),std::abs(nextSine.hi));
    const double cosineRemainder=std::max(std::abs(nextCosine.lo),std::abs(nextCosine.hi));
    return {sine+Interval(-sineRemainder,sineRemainder),
            cosine+Interval(-cosineRemainder,cosineRemainder)};
}

double PatchBound(const Grid& grid,int su,int sv,int pu,int pv,const Analytic& a) {
    Polynomial x(pu,pv),y(pu,pv),z(pu,pv),w(pu,pv);
    double minWeight=std::numeric_limits<double>::infinity();
    for(int i=0;i<=pu;++i) for(int j=0;j<=pv;++j) {
        const auto& h=grid[su-pu+i][sv-pv+j];
        x.At(i,j)=h[0]; y.At(i,j)=h[1]; z.At(i,j)=h[2]; w.At(i,j)=h[3];
        minWeight=std::min(minWeight,h[3].lo);
    }
    if(!(minWeight>0)) throw std::runtime_error("nonpositive rational weight enclosure");
    const Interval ax(a.axis.X()),ay(a.axis.Y()),az(a.axis.Z());
    const Interval axisNorm=SquareRoot(ax*ax+ay*ay+az*az);
    const Polynomial axial=Scale(x,ax/axisNorm)+Scale(y,ay/axisNorm)+Scale(z,az/axisNorm);
    Polynomial residual=axial;
    int degree=1;
    Interval denominator(1);
    bool sqrtBound=false;
    if(a.type!=GeomAbs_Plane) {
        const auto norm2=Square(x)+Square(y)+Square(z);
        const auto w2=Square(w);
        const auto radial2=a.type==GeomAbs_Sphere?norm2:norm2-Square(axial);
        const Interval r(a.radius);
        degree=2;
        if(a.type==GeomAbs_Sphere) { residual=norm2-Scale(w2,r*r); denominator=r; }
        else if(a.type==GeomAbs_Cylinder) { residual=radial2-Scale(w2,r*r); denominator=r; }
        else if(a.type==GeomAbs_Cone) {
            const auto trig=SineCosineEnclosures(a.angle);
            const Interval s=trig.first,c=trig.second;
            const auto q=Scale(w,r*c)+Scale(axial,s);
            residual=Scale(radial2,c*c)-Square(q);
            // d = |cos(a)*rho - |r*cos(a)+z*sin(a)||. Since
            // (A-B)^2 <= |A^2-B^2| for A,B >= 0, sqrt(|F|) is
            // conservative even at the cone apex; no unstable division by rho.
            sqrtBound=true;
        } else {
            const Interval R(a.major);
            const auto s=norm2+Scale(w2,R*R-r*r);
            residual=Square(s)-Scale(radial2*w2,Interval(4)*R*R);
            degree=4;
            // F=(a^2-r^2)(b^2-r^2), a=distance to generating
            // circle's center in the meridian, b^2=(rho+R)^2+z^2.
            // |F| >= d * r * (R^2-r^2) for a ring torus R>r.
            denominator=r*(R*R-r*r);
        }
    }
    double maxResidual=0;
    for(const auto c:residual.c) maxResidual=std::max(maxResidual,std::max(std::abs(c.lo),std::abs(c.hi)));
    Interval weightPower(1);
    for(int i=0;i<degree;++i) weightPower=weightPower*Interval(minWeight);
    Interval bound=Interval(maxResidual)/weightPower;
    if(sqrtBound) bound=SquareRoot(bound);
    else bound=bound/denominator;
    return bound.hi;
}
} // namespace

ImplicitCertificateResult CertifyImplicitDistance(
    const Handle(Geom_BSplineSurface)& source,const Handle(Geom_Surface)& analytic) {
    ImplicitCertificateResult result;
    try {
        if(source.IsNull()||analytic.IsNull()) throw std::runtime_error("null certificate surface");
        // Even the identity shortcut requires a defined geometric input.
        // A NaN control point cannot acquire a zero-distance certificate just
        // because the caller supplied the same handle twice.
        for(int i=1;i<=source->NbUPoles();++i) for(int j=1;j<=source->NbVPoles();++j) {
            const auto p=source->Pole(i,j);const double w=source->Weight(i,j);
            if(!std::isfinite(p.X())||!std::isfinite(p.Y())||!std::isfinite(p.Z())||!std::isfinite(w)||w<=0)
                throw std::runtime_error("invalid certificate control point or rational weight");
        }
        if(source==analytic) {
            result.certified=true; result.upperBound=0;
            result.message="identical supporting surface identity";
            return result;
        }
        const int pu=source->UDegree(),pv=source->VDegree();
        if(pu>12||pv>12) throw std::runtime_error("certificate degree budget exceeded (maximum 12)");
        const Analytic target=Describe(analytic);
        auto uk=ExpandedKnots(source,true),vk=ExpandedKnots(source,false);
        const int nu=static_cast<int>(uk.size())-pu-1,nv=static_cast<int>(vk.size())-pv-1;
        if(nu<source->NbUPoles()||nv<source->NbVPoles()||
           (!source->IsUPeriodic()&&nu!=source->NbUPoles())||
           (!source->IsVPeriodic()&&nv!=source->NbVPoles()))
            throw std::runtime_error("invalid virtual periodic pole count");
        Grid grid(nu,std::vector<HPoint>(nv));
        for(int i=1;i<=nu;++i) for(int j=1;j<=nv;++j) {
            const int iu=(i-1)%source->NbUPoles()+1,iv=(j-1)%source->NbVPoles()+1;
            const auto p=source->Pole(iu,iv);
            const Interval w(source->Weight(iu,iv));
            if(w.lo<=0) throw std::runtime_error("certificate requires positive rational weights");
            grid[i-1][j-1]={{(Interval(p.X())-Interval(target.origin.X()))*w,
                            (Interval(p.Y())-Interval(target.origin.Y()))*w,
                            (Interval(p.Z())-Interval(target.origin.Z()))*w,w}};
        }
        BezierKnots(grid,uk,pu);
        Transpose(grid); BezierKnots(grid,vk,pv); Transpose(grid);
        double maximum=0;
        for(int u=pu;u<static_cast<int>(grid.size());++u) if(uk[u]<uk[u+1])
            for(int v=pv;v<static_cast<int>(grid[0].size());++v) if(vk[v]<vk[v+1]) {
                maximum=std::max(maximum,PatchBound(grid,u,v,pu,pv,target));
                ++result.patchCount;
            }
        if(result.patchCount==0||!std::isfinite(maximum)) throw std::runtime_error("empty or invalid certificate domain");
        result.certified=true;
        result.upperBound=maximum;
        result.message="outward-rounded rational Bernstein full-domain directed bound";
    } catch(const std::exception& e) { result.message=e.what(); }
      catch(...) { result.message="geometry kernel failed during full-domain certification"; }
    return result;
}
} // namespace BS2Quad
