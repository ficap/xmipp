#include <gtest/gtest.h>
#include "alignment_test_utils.h"
#include "reconstruction/iterative_alignment_estimator.h"
//#include "reconstruction/polar_rotation_estimator.h"
//#include "reconstruction/shift_corr_estimator.h"
#include "reconstruction_cuda/cuda_rot_polar_estimator.h"
#include "reconstruction_cuda/cuda_shift_corr_estimator.h"
#include "core/utils/time_utils.h"
#include "core/utils/memory_utils.h"


template<typename T>
class IterativeAlignmentEstimatorHelper : public Alignment::IterativeAlignmentEstimator<T> {
public:
    static void applyTransform(const Dimensions &dims, const std::vector<Point2D<float>> &shifts, const std::vector<float> &rotations,
                const T * __restrict__ orig, T * __restrict__ copy) {
            Alignment::AlignmentEstimation e(dims.n());
            for (size_t i = 0; i < dims.n(); ++i) {
                auto &m = e.poses.at(i);
                auto &s = shifts.at(i);
                MAT_ELEM(m,0,2) += s.x;
                MAT_ELEM(m,1,2) += s.y;
                auto r = Matrix2D<double>();
                rotation2DMatrix(rotations.at(i), r);
                m = r * m;
            }
            Alignment::IterativeAlignmentEstimator<T>::sApplyTransform(dims, e, orig, copy, true);
        }
};

template<typename T>
class IterativeAlignmentEstimator_Test : public ::testing::Test {
public:
    void generateAndTest2D(size_t n, size_t batch) {
//        std::uniform_int_distribution<> dist1(0, 368);
//        std::uniform_int_distribution<> dist2(369, 768);
//
//        // only even inputs are valid
//        // smaller
//        size_t size = ((int)dist1(mt) / 2) * 2;
//        auto desc = std::to_string(size) + "x" + std::to_string(size) + "x" + std::to_string(n) + "(batch " + std::to_string(batch) + ")";
//        timeUtils::reportTimeMs(desc, [&]{
//            test(Dimensions(size, size, 1, n), batch);
//        });shifts
        // bigger
//
        size_t size = 32;//((int)dist2(mt) / 2) * 2;
        auto desc = std::to_string(size) + "x" + std::to_string(size) + "x" + std::to_string(n) + "(batch " + std::to_string(batch) + ")";
//        timeUtils::reportTimeMs(desc, [&]{
//            test(Dimensions(size, size, 1, n), batch);
//        });

        size = 64;//((int)dist2(mt) / 2) * 2;
        desc = std::to_string(size) + "x" + std::to_string(size) + "x" + std::to_string(n) + "(batch " + std::to_string(batch) + ")";
        timeUtils::reportTimeMs(desc, [&]{
            test(Dimensions(size, size, 1, n), batch);
        });

//         size = 96;//((int)dist2(mt) / 2) * 2;
//         desc = std::to_string(size) + "x" + std::nto_string(size) + "x" + std::to_string(n) + "(batch " + std::to_string(batch) + ")";
//        timeUtils::reportTimeMs(desc, [&]{
//            test(Dimensions(size, size, 1, n), batch);
//        });
//
//
//         size = 128;//((int)dist2(mt) / 2) * 2;
//         desc = std::to_string(size) + "x" + std::to_string(size) + "x" + std::to_string(n) + "(batch " + std::to_string(batch) + ")";
//        timeUtils::reportTimeMs(desc, [&]{
//            test(Dimensions(size, size, 1, n), batch);
//        });


//         size = 160;//((int)dist2(mt) / 2) * 2;
//         desc = std::to_string(size) + "x" + std::to_string(size) + "x" + std::to_string(n) + "(batch " + std::to_string(batch) + ")";
//        timeUtils::reportTimeMs(desc, [&]{
//            test(Dimensions(size, size, 1, n), batch);
//        });
//
//         size = 192;//((int)dist2(mt) / 2) * 2;
//         desc = std::to_string(size) + "x" + std::to_string(size) + "x" + std::to_string(n) + "(batch " + std::to_string(batch) + ")";
//        timeUtils::reportTimeMs(desc, [&]{
//            test(Dimensions(size, size, 1, n), batch);
//        });
//
//         size = 224;//((int)dist2(mt) / 2) * 2;
//         desc = std::to_string(size) + "x" + shiftsstd::to_string(size) + "x" + std::to_string(n) + "(batch " + std::to_string(batch) + ")";
//        timeUtils::reportTimeMs(desc, [&]{
//            test(Dimensions(size, size, 1, n), batch);
//        });
//
//         size = 256;//((int)dist2(mt) / 2) * 2;
//         desc = std::to_string(size) + "x" + std::to_string(size) + "x" + std::to_string(n) + "(batch " + std::to_string(batch) + ")";
//        timeUtils::reportTimeMs(desc, [&]{
//            test(Dimensions(size, size, 1, n), batch);
//        });



    }

    void test(const Dimensions &dims, size_t batch) {
        using namespace Alignment;

        auto maxShift = std::min((size_t)20, getMaxShift(dims));
        auto shifts = generateShifts(dims, maxShift, mt);
        auto maxRotation = getMaxRotation();
        auto rotations = generateRotations(dims, maxRotation, mt);

        auto maxSize = Dimensions(512, 512, 1, 10000);
        assert(dims.size() < maxSize.size());

        static auto others = new T[maxSize.size()]();
        static auto ref = memoryUtils::page_aligned_alloc<T>(maxSize.sizeSingle(), true);
        T centerX = dims.x() / 2;
        T centerY = dims.y() / 2;
        drawClockArms(ref, dims, centerX, centerY, 0.f);

        IterativeAlignmentEstimatorHelper<T>::applyTransform(
                dims, shifts, rotations, ref, others);
//        outputData(others, dims);

        // prepare aligner
//        auto cpu = CPU(2);
//        auto shiftAligner = ShiftCorrEstimator<T>();
//        auto rotationAligner = PolarRotationEstimator<T>();
        auto gpu1 = GPU();
        auto gpu2 = GPU();
        gpu1.set();
        gpu2.set();
        auto hw = std::vector<HW*>{&gpu1, &gpu2};
        auto rotSettings = Alignment::RotationEstimationSetting();
        rotSettings.hw = hw;
        rotSettings.type = AlignType::OneToN;
        rotSettings.refDims = dims.createSingle();
        rotSettings.otherDims = dims;
        rotSettings.batch = batch;
        rotSettings.maxRotDeg = maxRotation;
        rotSettings.fullCircle = true;


        auto shiftAligner = CudaShiftCorrEstimator<T>();
        auto rotationAligner = CudaRotPolarEstimator<T>();
        shiftAligner.init2D(hw, AlignType::OneToN, FFTSettingsNew<T>(dims, batch), maxShift, true, true);
        rotationAligner.init(rotSettings, false); // FIXME DS add test that batch is 1; set reuse=true
        auto aligner = IterativeAlignmentEstimator<T>(rotationAligner, shiftAligner);

        auto result  = aligner.compute(ref, others);

//        for (size_t i = 0; i < dims.n(); ++i) {
//            auto sE = shifts.at(i);
//            auto rE = rotations.at(i);
//            auto m = result.poses.at(i);
//            auto sA = Point2D<float>(-MAT_ELEM(m, 0, 2), -MAT_ELEM(m, 1, 2));
//            auto rA = fmod(360 + RAD2DEG(atan2(MAT_ELEM(m, 1, 0), MAT_ELEM(m, 0, 0))), 360);
//
////            printf("exp: | %f | %f | %f | act: | %f | %f | %f ",
////                    sE.x, sE.y, rE,
////                    sA.x, sA.y, rA
////            );
//
////            size_t offset = i * dims.xyzPadded();
////            auto M = getReferenceTransform(ref, others + offset, dims);
////            auto sR = Point2D<float>(-MAT_ELEM(M, 0, 2), -MAT_ELEM(M, 1, 2));
////            auto rR = fmod(360 + RAD2DEG(atan2(MAT_ELEM(M, 1, 0), MAT_ELEM(M, 0, 0))), 360);
////            printf("| ref: | %f | %f | %f\n",
////                    sR.x, sR.y, rR);
//
////            printf("\n");
//        }

        // don't delete data, try to reuse it
//        delete[] ref;
//        delete[] others;
    }


private:
    static std::mt19937 mt;

    void outputData(T *data, const Dimensions &dims) {
        MultidimArray<T>wrapper(dims.n(), dims.z(), dims.y(), dims.x(), data);
        Image<T> img(wrapper);
        img.write("data.stk");
    }

    Matrix2D<double> getReferenceTransform(T *ref, T *other, const Dimensions &dims) {
        Matrix2D<double> M;
        auto I = convert(other, dims);
        I.setXmippOrigin();
        auto refWrapper = convert(ref, dims);
        refWrapper.setXmippOrigin();

        alignImages(refWrapper, I, M, DONT_WRAP);
        return M;
    }

    MultidimArray<double> convert(float *data, const Dimensions &dims) {
        auto wrapper = MultidimArray<double>(1, 1, dims.y(), dims.x());
        for (size_t i = 0; i < dims.xyz(); ++i) {
            wrapper.data[i] = data[i];
        }
        return wrapper;
    }

    MultidimArray<double> convert(double *data, const Dimensions &dims) {
        return MultidimArray<double>(1, 1, dims.y(), dims.x(), data);
    }

};
TYPED_TEST_CASE_P(IterativeAlignmentEstimator_Test);

template<typename T>
std::mt19937 IterativeAlignmentEstimator_Test<T>::mt(42); // fixed seed to ensure reproducibility


//TYPED_TEST_P( IterativeAlignmentEstimator_Test, align2DOneToOne)
//{
//    XMIPP_TRY
//    // test one reference vs one image
//    IterativeAlignmentEstimator_Test<TypeParam>::generateAndTest2D(1, 1);
//    XMIPP_CATCH
//}

TYPED_TEST_P( IterativeAlignmentEstimator_Test, align2DOneToMany)
{
    XMIPP_TRY
    // test one reference vs one image
    IterativeAlignmentEstimator_Test<TypeParam>::generateAndTest2D(10000, 1000);
    XMIPP_CATCH
}

REGISTER_TYPED_TEST_CASE_P(IterativeAlignmentEstimator_Test,
//    align2DOneToOne,
    align2DOneToMany
);

typedef ::testing::Types<float> TestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, IterativeAlignmentEstimator_Test, TestTypes);