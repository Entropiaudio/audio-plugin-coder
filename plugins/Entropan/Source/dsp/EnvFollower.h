#pragma once

namespace entropan
{
    // Envelope follower (global detection circuit — feeds every Env-mode band).
    // Ballistics run per sample in EntropanAudioProcessor::processBlock, which
    // owns one of these as `env`.
    struct EnvFollowerState
    {
        float scLp  = 0.0f;   // sidechain HPF one-pole state
        float state = 0.0f;   // ballistics state
        float out   = 0.0f;   // 0..1, updated per sample from the input copy
    };
}
