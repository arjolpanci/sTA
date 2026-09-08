#pragma once
#include <algorithm>
#include <string>

// Simulation-owned clock: pausing stops animation; both render passes see one pose.
struct AnimationState {
    std::string clip="Idle", previous="Idle";
    float time=0, previousTime=0, blend=1;
    void update(const std::string& next,float dt,float rate=1) {
        if(next!=clip) {previous=clip;previousTime=time;clip=next;time=0;blend=0;}
        time+=dt*rate;previousTime+=dt*rate;blend=std::min(1.0f,blend+dt/.18f);
    }
};
