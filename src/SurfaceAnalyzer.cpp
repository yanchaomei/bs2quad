#include "SurfaceAnalyzer.h"
#include "AnalyticRecovery.h"
#include "ImplicitCertificate.h"
#include <BRep_Tool.hxx>
#include <algorithm>

namespace BS2Quad {
SurfaceAnalyzer::SurfaceAnalyzer(double tolerance) : m_tolerance(tolerance),m_samplesU(16),m_samplesV(16) { SetTolerance(tolerance); }
SurfaceAnalyzer::~SurfaceAnalyzer() = default;
void SurfaceAnalyzer::SetTolerance(double value) {
    if(!std::isfinite(value)||value<=0) throw std::invalid_argument("tolerance must be finite and positive");
    m_tolerance=value;
}
void SurfaceAnalyzer::SetSampleSize(int u,int v) {
    m_samplesU=std::clamp(u,Sampling::MIN_SAMPLES_U,Sampling::MAX_SAMPLES_U);
    m_samplesV=std::clamp(v,Sampling::MIN_SAMPLES_V,Sampling::MAX_SAMPLES_V);
}
SurfaceAnalysisResult SurfaceAnalyzer::Analyze(const Handle(Geom_BSplineSurface)& source) {
    SurfaceAnalysisResult result;
    m_samplePoints.clear();m_uParams.clear();m_vParams.clear();
    result.error=CheckNurbsInput(source,result.message);
    if(result.error!=ErrorType::SUCCESS) return result;
    double u0,u1,v0,v1;source->Bounds(u0,u1,v0,v1);
    try {
        for(int i=0;i<m_samplesU;++i) for(int j=0;j<m_samplesV;++j) {
            const double u=u0+(u1-u0)*i/(m_samplesU-1);
            const double v=v0+(v1-v0)*j/(m_samplesV-1);
            m_samplePoints.push_back(source->Value(u,v));
            m_uParams.push_back(u);m_vParams.push_back(v);
        }
        const auto candidate=RecoverAnalytic(source,m_tolerance);
        if(candidate.surface.IsNull()) { result.error=ErrorType::NON_QUADRIC;result.message=candidate.message;return result; }
        const auto bound=CertifyImplicitDistance(source,candidate.surface);
        if(!bound.certified||bound.upperBound>m_tolerance) {
            result.error=ErrorType::HIGH_ERROR;result.message="candidate rejected: "+bound.message;return result;
        }
        result.type=candidate.type;
        // A deterministic acceptance indicator, not a calibrated probability.
        result.confidence=1.0;result.message="analytic type independently bounded on full NURBS domain";
    } catch(const Standard_Failure& failure) {
        result.error=ErrorType::DATA_CORRUPT;result.message=failure.GetMessageString()?failure.GetMessageString():"kernel failure";
    }
    return result;
}
SurfaceAnalysisResult SurfaceAnalyzer::Analyze(const TopoDS_Face& face) {
    if(face.IsNull()) {SurfaceAnalysisResult r;r.error=ErrorType::DATA_CORRUPT;r.message="null face";return r;}
    return Analyze(Handle(Geom_BSplineSurface)::DownCast(BRep_Tool::Surface(face)));
}
}
