#include "AnalyticRecovery.h"
#include <GeomConvert_SurfToAnaSurf.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Standard_Failure.hxx>

namespace BS2Quad {
namespace {
GeomAbs_SurfaceType KernelType(QuadricType type) {
    switch(type) {
    case QuadricType::PLANE:return GeomAbs_Plane;
    case QuadricType::CYLINDER:return GeomAbs_Cylinder;
    case QuadricType::CONE:return GeomAbs_Cone;
    case QuadricType::SPHERE:return GeomAbs_Sphere;
    case QuadricType::TORUS:return GeomAbs_Torus;
    default:return GeomAbs_OtherSurface;
    }
}
QuadricType PublicType(GeomAbs_SurfaceType type) {
    switch(type) {
    case GeomAbs_Plane:return QuadricType::PLANE;
    case GeomAbs_Cylinder:return QuadricType::CYLINDER;
    case GeomAbs_Cone:return QuadricType::CONE;
    case GeomAbs_Sphere:return QuadricType::SPHERE;
    case GeomAbs_Torus:return QuadricType::TORUS;
    default:return QuadricType::NONE;
    }
}
}

ErrorType CheckNurbsInput(const Handle(Geom_BSplineSurface)& source,std::string& message) {
    if(source.IsNull()) {message="DATA_CORRUPT: null NURBS surface";return ErrorType::DATA_CORRUPT;}
    double u0,u1,v0,v1;
    source->Bounds(u0,u1,v0,v1);
    if(!std::isfinite(u0)||!std::isfinite(u1)||!std::isfinite(v0)||!std::isfinite(v1)||u0>=u1||v0>=v1) {
        message="OUT_OF_RANGE: nonfinite or empty parameter domain";return ErrorType::OUT_OF_RANGE;
    }
    for(int i=1;i<=source->NbUPoles();++i) for(int j=1;j<=source->NbVPoles();++j) {
        const auto& p=source->Pole(i,j);
        const double w=source->Weight(i,j);
        if(!std::isfinite(p.X())||!std::isfinite(p.Y())||!std::isfinite(p.Z())||!std::isfinite(w)||w<=0) {
            message="DATA_CORRUPT: invalid control point or nonpositive rational weight";
            return ErrorType::DATA_CORRUPT;
        }
    }
    for(int direction=0;direction<2;++direction) {
        const int count=direction==0?source->NbUKnots():source->NbVKnots();
        double previous=-std::numeric_limits<double>::infinity();
        for(int i=1;i<=count;++i) {
            const double knot=direction==0?source->UKnot(i):source->VKnot(i);
            if(!std::isfinite(knot)||knot<=previous) {
                message="DATA_CORRUPT: invalid distinct knot sequence";return ErrorType::DATA_CORRUPT;
            }
            previous=knot;
        }
    }
    message="input valid";
    return ErrorType::SUCCESS;
}

AnalyticCandidate RecoverAnalytic(const Handle(Geom_BSplineSurface)& source,
    double tolerance,QuadricType target) {
    if(!std::isfinite(tolerance)||tolerance<=0) throw std::invalid_argument("tolerance must be finite and positive (mm)");
    AnalyticCandidate result;
    try {
        result.error=CheckNurbsInput(source,result.message);
        if(result.error!=ErrorType::SUCCESS) return result;
        result.error=ErrorType::NON_QUADRIC;
        double u0,u1,v0,v1;source->Bounds(u0,u1,v0,v1);
        GeomConvert_SurfToAnaSurf recovery(source);
        if(target!=QuadricType::NONE) {
            recovery.SetConvType(GeomConvert_Target);
            recovery.SetTarget(KernelType(target));
        }
        result.surface=recovery.ConvertToAnalytical(tolerance,u0,u1,v0,v1);
        if(result.surface.IsNull()) {result.message="NON_QUADRIC: no analytic candidate at requested tolerance";return result;}
        result.type=PublicType(GeomAdaptor_Surface(result.surface).GetType());
        result.kernelGap=recovery.Gap();
        if(result.type==QuadricType::NONE||(target!=QuadricType::NONE&&result.type!=target)) {
            result.surface.Nullify();result.type=QuadricType::NONE;
            result.message="NON_QUADRIC: candidate has unexpected surface type";return result;
        }
        result.message="analytic candidate recovered; independent validation required";
        result.error=ErrorType::SUCCESS;
    } catch(const Standard_Failure& failure) {
        result.surface.Nullify();result.type=QuadricType::NONE;
        result.error=ErrorType::UNKNOWN_ERROR;
        result.message=std::string("KERNEL_FAILURE: ")+(failure.GetMessageString()?failure.GetMessageString():"OCCT exception");
    } catch(const std::exception& failure) {
        result.surface.Nullify();result.type=QuadricType::NONE;result.message=std::string("RECOVERY_FAILURE: ")+failure.what();
        result.error=ErrorType::UNKNOWN_ERROR;
    }
    return result;
}
}
