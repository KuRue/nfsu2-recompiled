#pragma once
#include <algorithm>
#include <array>
#include <cmath>
// Bounded window of completed presentation intervals; no per-frame allocation.
template<unsigned Capacity=120>
class FrameMetrics {
    std::array<double,Capacity> samples{};
    unsigned count=0;
public:
    void add(double milliseconds){if(std::isfinite(milliseconds)&&milliseconds>0&&count<samples.size())samples[count++]=milliseconds;}
    struct Summary {double p50=0,p95=0,p99=0,maximum=0,jitter=0;unsigned late=0,count=0;};
    Summary take(unsigned limit){
        Summary result;result.count=count;
        if(count){
            std::sort(samples.begin(),samples.begin()+count);
            auto percentile=[&](unsigned p){return samples[(count*p+99)/100-1];};
            result.p50=percentile(50);result.p95=percentile(95);result.p99=percentile(99);result.maximum=samples[count-1];
            double deviation=0;for(unsigned i=0;i<count;++i)deviation+=std::fabs(samples[i]-result.p50);
            result.jitter=deviation/count;
            if(limit)for(unsigned i=0;i<count;++i)if(samples[i]>1500.0/limit)++result.late;
        }
        count=0;return result;
    }
};
