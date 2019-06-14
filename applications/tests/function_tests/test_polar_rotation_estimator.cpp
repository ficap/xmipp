#include "data/cpu.h"
#include "reconstruction/polar_rotation_estimator.h"

template<typename T>
class ARotationEstimator_Test;

#define SETUP \
    void SetUp() { \
        estimator = new Alignment::PolarRotationEstimator<T>(); \
    }

#define SETUPTESTCASE \
    static void SetUpTestCase() { \
        hw = new CPU(); \
        hw->set(); \
    }

#define INIT \
    ((Alignment::PolarRotationEstimator<T>*)estimator)->init2D(*hw, AlignType::OneToN, dims, batch, maxRotation);

#include "arotation_estimator_tests.h"

typedef ::testing::Types<float, double> TestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(Cpu, ARotationEstimator_Test, TestTypes);
