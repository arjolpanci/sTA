#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// A rolling gameplay-only window. Opening the menu freezes these measurements,
// so a fast paused frame cannot conceal a slow simulation/rendering frame.
class PerformanceHistory {
public:
    struct Sample { double frameMs=0, updateMs=0, renderMs=0; };
    struct Summary { size_t count=0; Sample mean; double p95Ms=0; };
    void record(bool playing, Sample sample) {
        if(!playing || !std::isfinite(sample.frameMs) || !std::isfinite(sample.updateMs) ||
           !std::isfinite(sample.renderMs) || sample.frameMs<=0 || sample.updateMs<0 || sample.renderMs<0)return;
        m_samples[m_next]=sample;m_next=(m_next+1)%m_samples.size();m_count=std::min(m_count+1,m_samples.size());
    }
    Summary summary() const {
        Summary result;result.count=m_count;if(!m_count)return result;
        std::array<double,120> times{};
        for(size_t i=0;i<m_count;++i) {
            result.mean.frameMs+=m_samples[i].frameMs;result.mean.updateMs+=m_samples[i].updateMs;
            result.mean.renderMs+=m_samples[i].renderMs;times[i]=m_samples[i].frameMs;
        }
        result.mean.frameMs/=m_count;result.mean.updateMs/=m_count;result.mean.renderMs/=m_count;
        std::sort(times.begin(),times.begin()+m_count);result.p95Ms=times[size_t(std::ceil(m_count*.95))-1];
        return result;
    }
private:
    std::array<Sample,120> m_samples{};
    size_t m_count=0,m_next=0;
};
