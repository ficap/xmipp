/***************************************************************************
 *
 * Authors:     David Strelak (davidstrelak@gmail.com)
 *
 * Unidad de  Bioinformatica of Centro Nacional de Biotecnologia , CSIC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA
 *
 *  All comments concerning this program package may be sent to the
 *  e-mail address 'xmipp@cnb.csic.es'
 ***************************************************************************/

#include "aalign_significant.h"

namespace Alignment {

template<typename T>
void AProgAlignSignificant<T>::defineParams() {
    addUsageLine("Find alignment of the experimental images in respect to a set of references");

    addParamsLine("   -i <md_file>                : Metadata file with the experimental images");
    addParamsLine("   -r <md_file>                : Metadata file with the reference images");
    addParamsLine("   -o <md_file>                : Resulting metadata file with the aligned images");
    addParamsLine("   [--thr <N=-1>]              : Maximal number of the processing CPU threads");
    addParamsLine("   [--angDistance <a=10>]      : Angular distance");
    addParamsLine("   [--odir <outputDir=\".\">]  : Output directory");
    addParamsLine("   [--keepBestN <N=1>]         : For each image, store N best alignments to references. N must be smaller than no. of references");
    addParamsLine("   [--allowInputSwap]          : Allow swapping reference and experimental images");
    addParamsLine("   [--useWeightInsteadOfCC]    : Select the best reference using weight, instead of CC");
}

template<typename T>
void AProgAlignSignificant<T>::readParams() {
    m_imagesToAlign.fn = getParam("-i");
    m_referenceImages.fn = getParam("-r");
    m_fnOut = std::string(getParam("--odir")) + "/" + std::string(getParam("-o"));
    m_angDistance = getDoubleParam("--angDistance");
    m_noOfBestToKeep = getIntParam("--keepBestN");
    m_allowDataSwap = checkParam("--allowInputSwap");
    m_useWeightInsteadOfCC = checkParam("--useWeightInsteadOfCC");

    int threads = getIntParam("--thr");
    if (-1 == threads) {
        m_settings.cpuThreads = CPU::findCores();
    } else {
        m_settings.cpuThreads = threads;
    }
}

template<typename T>
void AProgAlignSignificant<T>::show() const {
    if (verbose < 1) return;

    std::cout << "Input metadata              : " << m_imagesToAlign.fn << "\n";
    std::cout << "Reference metadata          : " << m_referenceImages.fn <<  "\n";
    std::cout << "Output metadata             : " << m_fnOut <<  "\n";
    std::cout << "Angular distance            : " << m_angDistance <<  "\n";
    std::cout << "Best references kept        : " << m_noOfBestToKeep << "\n";
    std::cout.flush();
}

template<typename T>
void AProgAlignSignificant<T>::load(DataHelper &h) {
    auto &md = h.md;
    md.read(h.fn);
    md.removeDisabled();

    size_t Xdim;
    size_t Ydim;
    size_t Zdim;
    size_t Ndim;
    getImageSize(h.fn, Xdim, Ydim, Zdim, Ndim);
    Ndim = md.size(); // FIXME DS why we  didn't get right Ndim from the previous call?
    auto dims = Dimensions(Xdim, Ydim, Zdim, Ndim);
    auto dimsCropped = Dimensions((Xdim / 2) * 2, (Ydim / 2) * 2, Zdim, Ndim);
    bool mustCrop = (dims != dimsCropped);
    h.dims = dimsCropped;

    // FIXME DS clean up the cropping routine somehow
    h.data = std::unique_ptr<T[]>(new T[dims.size()]);
    auto ptr = h.data.get();
    // routine loading the actual content of the images
    auto routine = [&dims, ptr]
            (int thrId, const FileName &fn, size_t storeIndex) {
        size_t offset = storeIndex * dims.sizeSingle();
        MultidimArray<T> wrapper(1, dims.z(), dims.y(), dims.x(), ptr + offset);
        auto img = Image<T>(wrapper);
        img.read(fn);
    };

    std::vector<Image<T>> tmpImages;
    tmpImages.reserve(m_threadPool.size());
    if (mustCrop) {
        std::cerr << "We need an even input (sizes must be multiple of two). Input will be cropped\n";
        for (size_t t = 0; t < m_threadPool.size(); ++t) {
            tmpImages.emplace_back(Xdim, Ydim);
        }
    }
    // routine loading the actual content of the images
    auto routineCrop = [&dims, &dimsCropped, ptr, &tmpImages]
            (int thrId, const FileName &fn, size_t storeIndex) {
        // load image
        tmpImages.at(thrId).read(fn);
        // copy just the part we're interested in
        const size_t destOffsetN = storeIndex * dimsCropped.sizeSingle();
        for (size_t y = 0; y < dimsCropped.y(); ++y) {
            size_t srcOffsetY = y * dims.x();
            size_t destOffsetY = y * dimsCropped.x();
            memcpy(ptr + destOffsetN + destOffsetY,
                    tmpImages.at(thrId).data.data + srcOffsetY,
                    dimsCropped.x() * sizeof(T));
        }
    };

    // make sure that the files are well-defined
    bool isValid = md.containsLabel(MDL_IMAGE)
        && md.containsLabel(MDL_ANGLE_ROT) && md.containsLabel(MDL_ANGLE_TILT);
    if ( ! isValid) {
        REPORT_ERROR(ERR_MD, h.fn + ": at least one of the following label is missing: MDL_IMAGE, MDL_ANGLE_ROT, MDL_ANGLE_TILT");
    }

    // load all images in parallel
    auto futures = std::vector<std::future<void>>();
    futures.reserve(Ndim);
    h.rots.reserve(Ndim);
    h.tilts.reserve(Ndim);
    size_t i = 0;
    FOR_ALL_OBJECTS_IN_METADATA(md) {
        FileName fn;
        float rot;
        float tilt;
        md.getValue(MDL_IMAGE, fn, __iter.objId);
        md.getValue(MDL_ANGLE_ROT, rot,__iter.objId);
        md.getValue(MDL_ANGLE_TILT, tilt,__iter.objId);
        h.rots.emplace_back(rot);
        h.tilts.emplace_back(tilt);
        if (mustCrop) {
            futures.emplace_back(m_threadPool.push(routineCrop, fn, i));
        } else {
            futures.emplace_back(m_threadPool.push(routine, fn, i));
        }
        i++;
    }
    // wait till done
    for (auto &f : futures) {
        f.get();
    }
}

template<typename T>
void AProgAlignSignificant<T>::check() const {
    if ( ! m_referenceImages.dims.equalExceptNPadded(m_imagesToAlign.dims)) {
        REPORT_ERROR(ERR_LOGIC_ERROR, "Dimensions of the images to align and reference images do not match");
    }
    if (m_noOfBestToKeep > m_referenceImages.dims.n()) {
        REPORT_ERROR(ERR_LOGIC_ERROR, "--keepBestN is higher than number of references");
    }
    if (m_referenceImages.dims.n() <= 1) {
        REPORT_ERROR(ERR_LOGIC_ERROR, "We need at least two references");
    }
}

template<typename T>
template<bool IS_ESTIMATION_TRANSPOSED>
void AProgAlignSignificant<T>::computeWeightsAndSave(
        const std::vector<AlignmentEstimation> &est,
        size_t refIndex) {
    const size_t noOfRefs = m_referenceImages.dims.n();
    const size_t noOfSignals = m_imagesToAlign.dims.n();

    // compute angle between two reference orientation
    auto getAngle = [&](size_t index) {
        return Euler_distanceBetweenAngleSets(
                m_referenceImages.rots.at(refIndex),
                m_referenceImages.tilts.at(refIndex),
                0.f,
                m_referenceImages.rots.at(index),
                m_referenceImages.tilts.at(index),
                0.f,
                true);
    };

    // find out which references are sufficiently similar
    size_t count = 0;
    auto mask = std::vector<bool>(noOfRefs, false);
    for (size_t r = 0; r < noOfRefs; ++r) {
        if ((refIndex == r)
            || (getAngle(r) <= m_angDistance)) {
            mask.at(r) = true;
            count++;
        }
    }

    // allocate necessary memory
    auto correlations = std::vector<WeightCompHelper>();
    correlations.reserve(count * noOfSignals);

    // for all similar references
    for (size_t r = 0; r < noOfRefs; ++r) {
        if (mask.at(r)) {
            // get correlations of all signals
            for (size_t s = 0; s < noOfSignals; ++s) {
                if (IS_ESTIMATION_TRANSPOSED) {
                    correlations.emplace_back(est.at(s).correlations.at(r), r, s);
                } else {
                    correlations.emplace_back(est.at(r).correlations.at(s), r, s);
                }
            }
        }
    }
    computeWeightsAndSave(correlations, refIndex);
}

template<typename T>
void AProgAlignSignificant<T>::computeWeightsAndSave(
        std::vector<WeightCompHelper> &correlations,
        size_t refIndex) {
    const size_t noOfSignals = m_imagesToAlign.dims.n();
    auto weights = std::vector<float>(noOfSignals, 0); // zero weight by default
    const size_t noOfCorrelations = correlations.size();

    // sort ascending using correlation
    std::sort(correlations.begin(), correlations.end(),
            [](const WeightCompHelper &l, const WeightCompHelper &r) {
        return l.correlation < r.correlation;
    });
    auto invMaxCorrelation = 1.f / correlations.back().correlation;

    // set weight for all images
    for (size_t c = 0; c < noOfCorrelations; ++c) {
        const auto &tmp = correlations.at(c);
        if (tmp.refIndex != refIndex) {
            continue; // current record is for different reference
        }
        // cumulative density function - probability of having smaller value then the rest
        float cdf = c / (float)(noOfCorrelations - 1); // <0..1> // won't work if we have just one reference
        float correlation = tmp.correlation;
        if (correlation > 0.f) {
            weights.at(tmp.imgIndex) = correlation * invMaxCorrelation * cdf;
        }
    }
    // store result
    m_weights.at(refIndex) = weights;
}

template<typename T>
template<bool IS_ESTIMATION_TRANSPOSED>
void AProgAlignSignificant<T>::computeWeights(
        const std::vector<AlignmentEstimation> &est) {
    const size_t noOfRefs = m_referenceImages.dims.n();
    m_weights.resize(noOfRefs);

    // for all references
    for (size_t r = 0; r < noOfRefs; ++r) {
        computeWeightsAndSave<IS_ESTIMATION_TRANSPOSED>(est, r);
    }
}

template<typename T>
void AProgAlignSignificant<T>::fillRow(MDRow &row,
        const Matrix2D<double> &pose,
        size_t refIndex,
        double weight, double maxVote) {
    // get orientation
    bool flip;
    double scale;
    double shiftX;
    double shiftY;
    double psi;
    transformationMatrix2Parameters2D(
            pose.inv(), // we want to store inverse transform
            flip, scale,
            shiftX, shiftY,
            psi);
    // FIXME DS add check of max shift / rotation
    row.setValue(MDL_ENABLED, 1);
    row.setValue(MDL_MAXCC, (double)maxVote);
    row.setValue(MDL_ANGLE_ROT, (double)m_referenceImages.rots.at(refIndex));
    row.setValue(MDL_ANGLE_TILT, (double)m_referenceImages.tilts.at(refIndex));
    // save both weight and weight significant, so that we can keep track of result of this
    // program, even after some other program re-weights the particle
    row.setValue(MDL_WEIGHT_SIGNIFICANT, weight);
    row.setValue(MDL_WEIGHT, weight);
    row.setValue(MDL_ANGLE_PSI, psi);
    row.setValue(MDL_SHIFT_X, -shiftX); // store negative translation
    row.setValue(MDL_SHIFT_Y, -shiftY); // store negative translation
    row.setValue(MDL_FLIP, flip);
    row.setValue(MDL_IMAGE_IDX, refIndex);
}

template<typename T>
void AProgAlignSignificant<T>::extractMax(
        std::vector<float> &data,
        size_t &pos, double &val) {
    using namespace ExtremaFinder;
    float p = 0;
    float v = 0;
    SingleExtremaFinder<T>::sFindMax(CPU(), Dimensions(data.size()), data.data(), &p, &v);
    pos = std::round(p);
    val = data.at(pos);
    data.at(pos) = std::numeric_limits<float>::lowest();
}

template<typename T>
template<bool IS_ESTIMATION_TRANSPOSED, bool USE_WEIGHT>
void AProgAlignSignificant<T>::storeAlignedImages(
        const std::vector<AlignmentEstimation> &est) {
    auto &md = m_imagesToAlign.md;
    auto result = MetaData();
    const size_t noOfRefs = m_referenceImages.dims.n();
    const auto dims = Dimensions(noOfRefs);

    auto accessor = [&](size_t image, size_t reference) {
        if (USE_WEIGHT) {
            return m_weights.at(reference).at(image);
        } else if (IS_ESTIMATION_TRANSPOSED) {
            return est.at(image).correlations.at(reference);
        } else {
            return est.at(reference).correlations.at(image);
        }
    };

    MDRow row;
    size_t i = 0;
    FOR_ALL_OBJECTS_IN_METADATA(md) {
        // get the original row from the input metadata
        md.getRow(row, __iter.objId);
        // collect voting from all references
        auto votes = std::vector<float>();
        votes.reserve(noOfRefs);
        for (size_t r = 0; r < noOfRefs; ++r) {
            votes.emplace_back(accessor(i, r));
        }
        double maxVote = std::numeric_limits<double>::lowest();
        // for all references that we want to store, starting from the best matching one
        for (size_t nthBest = 0; nthBest < m_noOfBestToKeep; ++nthBest) {
            size_t refIndex;
            double val;
            // get the max vote
            extractMax(votes, refIndex, val);
            if (0 == nthBest) {
                // set max vote that we found
                maxVote = val;
            }
            if (val <= 0) {
                continue; // skip saving the particles which have non-positive correlation to the reference
            }
            // update the row with proper pose info
            const auto &p = IS_ESTIMATION_TRANSPOSED
                    ? (est.at(i).poses.at(refIndex).inv())
                    : (est.at(refIndex).poses.at(i));
            fillRow(row,
                    p,
                    refIndex,
                    m_weights.at(refIndex).at(i),
                    maxVote); // best cross-correlation or weight
            // store it
            result.addRow(row);
        }
        i++;
    }
    result.write(m_fnOut);
}

template<typename T>
void AProgAlignSignificant<T>::updateSettings() {
    m_settings.refDims = m_referenceImages.dims;
    m_settings.otherDims = m_imagesToAlign.dims;
}

template<typename T>
void AProgAlignSignificant<T>::run() {
    show();
    m_threadPool.resize(getSettings().cpuThreads);
    // load data
    load(m_imagesToAlign);
    load(m_referenceImages);

    bool hasMoreReferences = m_allowDataSwap
            && (m_referenceImages.dims.n() > m_imagesToAlign.dims.n());
    if (hasMoreReferences) {
        std::cerr << "We are swapping reference images and experimental images. "
                "This will enhance the performance. The result should be equivalent, but not identical.\n";
        std::swap(m_referenceImages, m_imagesToAlign);
    }

    // for each reference, get alignment of all images
    updateSettings();
    check();
    auto alignment = align(m_referenceImages.data.get(), m_imagesToAlign.data.get());

    // process the alignment and store
    if (hasMoreReferences) {
        std::swap(m_referenceImages, m_imagesToAlign);
        computeWeights<true>(alignment);
        if (m_useWeightInsteadOfCC) {
            storeAlignedImages<true, true>(alignment);
        } else {
            storeAlignedImages<true, false>(alignment);
        }
    } else {
        computeWeights<false>(alignment);
        if (m_useWeightInsteadOfCC) {
            storeAlignedImages<false, true>(alignment);
        } else {
            storeAlignedImages<false, false>(alignment);
        }
    }
}

// explicit instantiation
template class AProgAlignSignificant<float>;

} /* namespace Alignment */
