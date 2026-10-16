#include "PointFitting.h"
#include <Geom_Plane.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <gp_Pln.hxx>
#include <gp_Sphere.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Cone.hxx>
#include <gp_Torus.hxx>
#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace BS2Quad {
namespace {
using Vec=std::array<double,3>;
using Matrix=std::vector<std::vector<double>>;
Vec Add(Vec a,Vec b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
Vec Sub(Vec a,Vec b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
Vec Scale(Vec a,double s){for(auto& x:a)x*=s;return a;}
double Dot(Vec a,Vec b){double s=0;for(int i=0;i<3;++i)s+=a[i]*b[i];return s;}
double Norm(Vec a){return std::sqrt(Dot(a,a));}
Vec Cross(Vec a,Vec b){return {{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};}
Vec Unit(Vec a){const double n=Norm(a);if(!std::isfinite(n)||n<1e-12)throw std::runtime_error("singular direction");return Scale(a,1/n);}
struct Eigen {std::vector<double> values;Matrix vectors;};
Eigen SymmetricEigen(Matrix a) {
    const int n=static_cast<int>(a.size());Matrix v(n,std::vector<double>(n));for(int i=0;i<n;++i)v[i][i]=1;
    for(int step=0;step<100*n*n;++step) {
        int p=0,q=1;double off=0,diagonal=0;
        for(int i=0;i<n;++i){diagonal=std::max(diagonal,std::abs(a[i][i]));for(int j=i+1;j<n;++j)if(std::abs(a[i][j])>off){off=std::abs(a[i][j]);p=i;q=j;}}
        if(off<=1e-15*std::max(1.0,diagonal))break;
        const double tau=(a[q][q]-a[p][p])/(2*a[p][q]);
        const double t=std::copysign(1.0,tau)/(std::abs(tau)+std::sqrt(1+tau*tau));
        const double c=1/std::sqrt(1+t*t),s=t*c;
        const double old=a[p][q];a[p][p]-=t*old;a[q][q]+=t*old;a[p][q]=a[q][p]=0;
        for(int i=0;i<n;++i)if(i!=p&&i!=q){const double x=a[i][p],y=a[i][q];a[i][p]=a[p][i]=c*x-s*y;a[i][q]=a[q][i]=s*x+c*y;}
        for(int i=0;i<n;++i){const double x=v[i][p],y=v[i][q];v[i][p]=c*x-s*y;v[i][q]=s*x+c*y;}
    }
    std::vector<int> order(n);for(int i=0;i<n;++i)order[i]=i;
    std::sort(order.begin(),order.end(),[&](int i,int j){return a[i][i]<a[j][j];});
    Eigen out;out.vectors=Matrix(n,std::vector<double>(n));
    for(int k:order){out.values.push_back(a[k][k]);for(int i=0;i<n;++i)out.vectors[i][out.values.size()-1]=v[i][k];}
    return out;
}
bool Solve(Matrix a,std::vector<double> b,std::vector<double>& x,double relative=1e-12) {
    const int n=static_cast<int>(b.size());double largest=0;for(const auto& row:a)for(double e:row)largest=std::max(largest,std::abs(e));
    if(!std::isfinite(largest)||largest==0)return false;
    for(int i=0;i<n;++i) {
        int pivot=i;for(int j=i+1;j<n;++j)if(std::abs(a[j][i])>std::abs(a[pivot][i]))pivot=j;
        if(std::abs(a[pivot][i])<largest*relative)return false;
        std::swap(a[i],a[pivot]);std::swap(b[i],b[pivot]);
        for(int j=i+1;j<n;++j){const double f=a[j][i]/a[i][i];for(int k=i;k<n;++k)a[j][k]-=f*a[i][k];b[j]-=f*b[i];}
    }
    x.assign(n,0);for(int i=n-1;i>=0;--i){double s=b[i];for(int j=i+1;j<n;++j)s-=a[i][j]*x[j];x[i]=s/a[i][i];if(!std::isfinite(x[i]))return false;}return true;
}
bool LeastSquares(const Matrix& rows,const std::vector<double>& values,std::vector<double>& x) {
    if(rows.empty())return false;
    const int n=static_cast<int>(rows[0].size());Matrix a(n,std::vector<double>(n));std::vector<double>b(n);
    for(std::size_t k=0;k<rows.size();++k)for(int i=0;i<n;++i){b[i]+=rows[k][i]*values[k];for(int j=0;j<n;++j)a[i][j]+=rows[k][i]*rows[k][j];}
    return Solve(a,b,x);
}
Vec Column(const Matrix& a,int index){return {{a[0][index],a[1][index],a[2][index]}};}
Vec ParameterVec(const std::vector<double>& p,int offset){return {{p[offset],p[offset+1],p[offset+2]}};}
std::pair<Vec,Vec> Basis(Vec axis){axis=Unit(axis);const Vec helper=std::abs(axis[0])<.8?Vec{{1,0,0}}:Vec{{0,1,0}};const Vec x=Unit(Cross(axis,helper));return {x,Cross(axis,x)};}

using Residual=std::function<double(const std::vector<double>&,Vec)>;
double Cost(const std::vector<Vec>& points,const std::vector<double>& parameters,const Residual& residual,std::vector<double>* values=nullptr) {
    double sum=0;if(values)values->clear();
    try {for(const auto& point:points){const double r=residual(parameters,point);if(!std::isfinite(r))return 1e100;sum+=r*r;if(values)values->push_back(r);}}
    catch(...){return 1e100;}return std::isfinite(sum)?sum:1e100;
}
void Refine(const std::vector<Vec>& points,std::vector<double>& p,const Residual& residual) {
    double cost=Cost(points,p,residual),lambda=1e-4;
    for(int iteration=0;iteration<100&&cost>1e-28;++iteration) {
        const int n=static_cast<int>(p.size());std::vector<double> values;Cost(points,p,residual,&values);if(values.size()!=points.size())break;
        Matrix jac(points.size(),std::vector<double>(n));
        for(int k=0;k<n;++k){auto plus=p,minus=p;const double step=1e-6*std::max(1.0,std::abs(p[k]));plus[k]+=step;minus[k]-=step;for(std::size_t i=0;i<points.size();++i)jac[i][k]=(residual(plus,points[i])-residual(minus,points[i]))/(2*step);}
        Matrix normal(n,std::vector<double>(n));std::vector<double> gradient(n);
        for(std::size_t i=0;i<points.size();++i)for(int j=0;j<n;++j){gradient[j]-=jac[i][j]*values[i];for(int k=0;k<n;++k)normal[j][k]+=jac[i][j]*jac[i][k];}
        for(int j=0;j<n;++j)normal[j][j]+=lambda*(1+normal[j][j]);
        std::vector<double> delta;if(!Solve(normal,gradient,delta,1e-18))break;
        auto trial=p;for(int j=0;j<n;++j)trial[j]+=delta[j];const double next=Cost(points,trial,residual);
        if(next<cost){p.swap(trial);cost=next;lambda=std::max(1e-14,lambda*.25);}else lambda=std::min(1e14,lambda*8);
    }
}
std::vector<double> Quadric(const std::vector<Vec>& points) {
    Matrix normal(10,std::vector<double>(10));
    for(const auto& p:points){const std::array<double,10> row={{p[0]*p[0],p[1]*p[1],p[2]*p[2],2*p[0]*p[1],2*p[0]*p[2],2*p[1]*p[2],2*p[0],2*p[1],2*p[2],1}};for(int i=0;i<10;++i)for(int j=0;j<10;++j)normal[i][j]+=row[i]*row[j];}
    const auto e=SymmetricEigen(normal);if(e.values[1]<1e-13*std::max(1.0,e.values.back()))return {};
    std::vector<double> q(10);for(int i=0;i<10;++i)q[i]=e.vectors[i][0];return q;
}
bool Circle(const std::vector<std::array<double,2>>& points,std::array<double,3>& circle) {
    Matrix rows;std::vector<double>b,x;for(const auto& p:points){rows.push_back({2*p[0],2*p[1],1});b.push_back(p[0]*p[0]+p[1]*p[1]);}
    if(!LeastSquares(rows,b,x))return false;
    const double r2=x[0]*x[0]+x[1]*x[1]+x[2];if(r2<=1e-14)return false;circle={{x[0],x[1],std::sqrt(r2)}};return true;
}
std::vector<double> CylinderSeed(const std::vector<Vec>& points,Vec axis) {
    const auto basis=Basis(axis);std::vector<std::array<double,2>> projected;
    for(const auto& p:points)projected.push_back({{Dot(p,basis.first),Dot(p,basis.second)}});
    std::array<double,3> circle;if(!Circle(projected,circle))return {};
    const Vec c=Add(Scale(basis.first,circle[0]),Scale(basis.second,circle[1]));
    return {c[0],c[1],c[2],axis[0],axis[1],axis[2],circle[2]};
}
// Signed geometric residuals to the infinite analytic supports. For a cone,
// rho*cos(a)-|z|*sin(a) is distance to the nearer nappe; the torus meridian
// distance hypot(rho-R,z)-r is exact for ring tori R>r.
Residual ResidualFor(QuadricType type) {
    return [type](const std::vector<double>& p,Vec x) {
        const auto d=Sub(x,ParameterVec(p,0));
        if(type==QuadricType::SPHERE){if(p[3]<=1e-8||p[3]>1e4)return 1e20;return Norm(d)-p[3];}
        const auto axis=Unit(ParameterVec(p,3));const double z=Dot(d,axis),rho=Norm(Sub(d,Scale(axis,z)));
        if(Norm(ParameterVec(p,0))>1e4)return 1e20;
        if(type==QuadricType::CYLINDER){if(p[6]<=1e-8||p[6]>1e4)return 1e20;return rho-p[6];}
        if(type==QuadricType::CONE){if(p[6]<1e-4||p[6]>M_PI/2-1e-4)return 1e20;return rho*std::cos(p[6])-std::abs(z)*std::sin(p[6]);}
        if(p[7]<=1e-8||p[6]<=p[7]*(1+1e-6)||p[6]>1e4)return 1e20;
        return std::hypot(rho-p[6],z)-p[7];
    };
}
PointFitResult ValidateWorldCandidate(const Handle(Geom_Surface)& surface,const std::vector<gp_Pnt>& points,
                                      QuadricType type,double tolerance) {
    PointFitResult result;
    gp_Pnt origin;gp_Dir direction(0,0,1);double radius=0,major=0,angle=0;
    if(type==QuadricType::PLANE){const auto p=Handle(Geom_Plane)::DownCast(surface)->Pln();origin=p.Location();direction=p.Axis().Direction();}
    if(type==QuadricType::SPHERE){const auto p=Handle(Geom_SphericalSurface)::DownCast(surface)->Sphere();origin=p.Location();radius=p.Radius();}
    if(type==QuadricType::CYLINDER){const auto p=Handle(Geom_CylindricalSurface)::DownCast(surface)->Cylinder();origin=p.Location();direction=p.Axis().Direction();radius=p.Radius();}
    if(type==QuadricType::CONE){const auto p=Handle(Geom_ConicalSurface)::DownCast(surface)->Cone();origin=p.Apex();direction=p.Axis().Direction();angle=p.SemiAngle();}
    if(type==QuadricType::TORUS){const auto p=Handle(Geom_ToroidalSurface)::DownCast(surface)->Torus();origin=p.Location();direction=p.Axis().Direction();radius=p.MinorRadius();major=p.MajorRadius();}
    const Vec axis=Unit({{direction.X(),direction.Y(),direction.Z()}});
    double maximum=0;long double squares=0;
    // Recheck the actual stored world-space primitive. Undoing normalization
    // can round the center/axis at large translations; a tiny normalized
    // residual must never bypass the physical point-distance acceptance gate.
    for(const auto& point:points) {
        const Vec d={{point.X()-origin.X(),point.Y()-origin.Y(),point.Z()-origin.Z()}};
        const double z=Dot(d,axis),rho=Norm(Sub(d,Scale(axis,z)));
        double residual=0;
        if(type==QuadricType::PLANE)residual=z;
        if(type==QuadricType::SPHERE)residual=Norm(d)-radius;
        if(type==QuadricType::CYLINDER)residual=rho-radius;
        if(type==QuadricType::CONE)residual=rho*std::cos(angle)-std::abs(z)*std::sin(angle);
        if(type==QuadricType::TORUS)residual=std::hypot(rho-major,z)-radius;
        if(!std::isfinite(residual))return result;
        maximum=std::max(maximum,std::abs(residual));squares+=static_cast<long double>(residual)*residual;
    }
    if(maximum>tolerance)return result;
    result.surface=surface;result.maximumResidual=maximum;result.rmsResidual=std::sqrt(static_cast<double>(squares/points.size()));
    return result;
}
} // namespace

PointFitResult FitFinitePoints(const std::vector<gp_Pnt>& points,QuadricType type,double tolerance,bool refine) {
    PointFitResult result;
    try {
        const std::size_t required=type==QuadricType::PLANE?3:type==QuadricType::SPHERE?4:type==QuadricType::CYLINDER?6:type==QuadricType::CONE?10:12;
        if(points.size()<required||!std::isfinite(tolerance)||tolerance<=0)return result;
        std::array<long double,3> sum={{0,0,0}};
        for(const auto& p:points){if(!std::isfinite(p.X())||!std::isfinite(p.Y())||!std::isfinite(p.Z()))return result;sum[0]+=p.X();sum[1]+=p.Y();sum[2]+=p.Z();}
        const Vec mean={{static_cast<double>(sum[0]/points.size()),static_cast<double>(sum[1]/points.size()),static_cast<double>(sum[2]/points.size())}};
        std::vector<Vec> cloud;long double square=0;
        for(const auto& p:points){const Vec x={{p.X()-mean[0],p.Y()-mean[1],p.Z()-mean[2]}};cloud.push_back(x);square+=Dot(x,x);}
        const double scale=std::sqrt(static_cast<double>(square/points.size()));if(!std::isfinite(scale)||scale<1e-12)return result;
        Matrix covariance(3,std::vector<double>(3));
        for(auto& p:cloud){p=Scale(p,1/scale);for(int i=0;i<3;++i)for(int j=0;j<3;++j)covariance[i][j]+=p[i]*p[j]/cloud.size();}
        const auto eigen=SymmetricEigen(covariance);
        if(eigen.values[1]<1e-12 || (type!=QuadricType::PLANE&&eigen.values[0]<1e-12))return result;
        const auto world=[&](Vec p){p=Add(mean,Scale(p,scale));return gp_Pnt(p[0],p[1],p[2]);};
        if(type==QuadricType::PLANE) {
            const auto normal=Unit(Column(eigen.vectors,0));double maximum=0;
            for(const auto& p:cloud){const double r=Dot(p,normal)*scale;maximum=std::max(maximum,std::abs(r));}
            if(maximum>tolerance)return result;
            result.surface=new Geom_Plane(world({{0,0,0}}),gp_Dir(normal[0],normal[1],normal[2]));return ValidateWorldCandidate(result.surface,points,type,tolerance);
        }
        const auto residual=ResidualFor(type);std::vector<std::vector<double>> seeds;
        if(type==QuadricType::SPHERE) {
            Matrix rows;std::vector<double>b,p;
            for(const auto& x:cloud){rows.push_back({2*x[0],2*x[1],2*x[2],1});b.push_back(Dot(x,x));}
            if(!LeastSquares(rows,b,p))return result;
            const double r2=p[0]*p[0]+p[1]*p[1]+p[2]*p[2]+p[3];if(r2<=0)return result;p[3]=std::sqrt(r2);seeds.push_back(p);
        } else if(type==QuadricType::CYLINDER) {
            for(int k=0;k<3;++k){auto p=CylinderSeed(cloud,Column(eigen.vectors,k));if(!p.empty())seeds.push_back(p);}
            const auto q=Quadric(cloud);
            if(!q.empty()){Matrix a={{q[0],q[3],q[4]},{q[3],q[1],q[5]},{q[4],q[5],q[2]}};const auto e=SymmetricEigen(a);int k=0;for(int j=1;j<3;++j)if(std::abs(e.values[j])<std::abs(e.values[k]))k=j;auto p=CylinderSeed(cloud,Column(e.vectors,k));if(!p.empty())seeds.push_back(p);}
        } else if(type==QuadricType::CONE) {
            // In a centered quadratic cone, apex=-A^{-1}b. Its unique-sign
            // eigenvector is the axis and -lambda_axis/lambda_radial=tan²(a).
            const auto q=Quadric(cloud);if(q.empty())return result;
            Matrix a={{q[0],q[3],q[4]},{q[3],q[1],q[5]},{q[4],q[5],q[2]}};std::vector<double> apex;
            if(!Solve(a,{-q[6],-q[7],-q[8]},apex))return result;
            const auto e=SymmetricEigen(a);
            for(int k=0;k<3;++k){const double radial=(e.values[(k+1)%3]+e.values[(k+2)%3])*.5;if(radial*e.values[k]>=0)continue;const double angle=std::atan(std::sqrt(-e.values[k]/radial));const auto axis=Column(e.vectors,k);seeds.push_back({apex[0],apex[1],apex[2],axis[0],axis[1],axis[2],angle});}
        } else if(type==QuadricType::TORUS) {
            // Try every covariance axis, not just the dominant one. Fit the
            // generating circle in (radial-distance, axial-coordinate) to
            // recover tube radius, rather than using radial standard deviation.
            std::vector<Vec> axes;for(int k=0;k<3;++k)axes.push_back(Column(eigen.vectors,k));axes.push_back({{1,0,0}});axes.push_back({{0,1,0}});axes.push_back({{0,0,1}});
            for(const auto& axis:axes){const auto cylinder=CylinderSeed(cloud,axis);if(cylinder.empty())continue;const auto center=ParameterVec(cylinder,0);std::vector<std::array<double,2>> meridian;for(const auto& x:cloud){const auto d=Sub(x,center);const double z=Dot(d,axis);meridian.push_back({{Norm(Sub(d,Scale(axis,z))),z}});}std::array<double,3> circle;if(!Circle(meridian,circle)||circle[0]<=circle[2])continue;const auto c=Add(center,Scale(axis,circle[1]));seeds.push_back({c[0],c[1],c[2],axis[0],axis[1],axis[2],circle[0],circle[2]});}
        } else return result;
        double best=std::numeric_limits<double>::infinity();std::vector<double> chosen;
        for(auto seed:seeds) {
            try {if(refine)Refine(cloud,seed,residual);double maximum=0;for(const auto& point:cloud){const double r=residual(seed,point);if(!std::isfinite(r)){maximum=std::numeric_limits<double>::infinity();break;}maximum=std::max(maximum,std::abs(r)*scale);}if(std::isfinite(maximum)&&maximum<best){best=maximum;chosen=seed;}}
            catch(...){continue;}
        }
        if(chosen.empty()||best>tolerance)return result;
        const auto center=world(ParameterVec(chosen,0));
        if(type==QuadricType::SPHERE)result.surface=new Geom_SphericalSurface(gp_Ax3(center,gp_Dir(0,0,1)),chosen[3]*scale);
        else {
            const auto axis=Unit(ParameterVec(chosen,3));const gp_Ax3 frame(center,gp_Dir(axis[0],axis[1],axis[2]));
            if(type==QuadricType::CYLINDER)result.surface=new Geom_CylindricalSurface(frame,chosen[6]*scale);
            if(type==QuadricType::CONE)result.surface=new Geom_ConicalSurface(frame,chosen[6],0);
            if(type==QuadricType::TORUS)result.surface=new Geom_ToroidalSurface(frame,chosen[6]*scale,chosen[7]*scale);
        }
        if(result.surface.IsNull())return result;
        return ValidateWorldCandidate(result.surface,points,type,tolerance);
    } catch(...) {result=PointFitResult();}
    return result;
}
} // namespace BS2Quad
