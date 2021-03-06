﻿#pragma once

#include "ICrownChinEstimator.h"

FWD_DECL(CrownChinEstimator)

class CrownChinEstimator : public ICrownChinEstimator
{
public:
    void configure(rapidjson::Value& config) override;
    bool estimateCrownChin(LandMarks& landmarks) override;

private:
     double m_chinCrownCoeff = 1.7699;
     double m_chinFrownCoeff = 0.8945;
};
