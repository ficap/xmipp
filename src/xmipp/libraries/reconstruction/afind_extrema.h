/***************************************************************************
 *
 * Authors:    David Strelak (davidstrelak@gmail.com)
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

#ifndef LIBRARIES_RECONSTRUCTION_AFIND_EXTREMA_H_
#define LIBRARIES_RECONSTRUCTION_AFIND_EXTREMA_H_

#include "data/dimensions.h"
#include "data/hw.h"
#include "data/point3D.h"
#include "core/xmipp_error.h"
#include <vector>
#include <cassert>

namespace ExtremaFinder {
enum class SearchType {
    Min, // in the whole signal (for each signal)
    Max, // in the whole signal (for each signal)
    MaxAroundCenter, // for each signal, search a circular area around center
    MaxNearCenter // for each signal, search a square area around center
};

enum class ResultType {
    Value,
    Position,
    Both
};

class ExtremaFinderSettings {
public:
    std::vector<HW*> hw;
    SearchType searchType;
    ResultType resultType;
    Dimensions dims = Dimensions(0);
    size_t batch;
    size_t maxDistFromCenter;

    Point3D<size_t> getCenter() const {
        return {dims.x() / 2, dims.y() / 2, dims.z() / 2};
    }

    void check() const {
        if (0 == hw.size()) {
            REPORT_ERROR(ERR_VALUE_INCORRECT, "HW contains zero (0) devices");
        }
        if ( ! dims.isValid()) {
            REPORT_ERROR(ERR_LOGIC_ERROR, "Dimensions are invalid (contain 0)");
        }
        if (0 == batch) {
            REPORT_ERROR(ERR_LOGIC_ERROR, "Batch is zero (0)");
        }
        if (SearchType::MaxAroundCenter == searchType) {
            auto center = getCenter();
            if (dims.is1D()) {
                if (maxDistFromCenter >= center.x) {
                    REPORT_ERROR(ERR_LOGIC_ERROR, "'maxDistFromCenter' is bigger than half of the signal's X dimension");
                }
            } else if (dims.is2D()) {
                if ((maxDistFromCenter >= center.x)
                        || (maxDistFromCenter >= center.y)) {
                    REPORT_ERROR(ERR_LOGIC_ERROR, "'maxDistFromCenter' is bigger than half of the signal's X or Y dimensions");
                }
            } else {
                if ((maxDistFromCenter >= center.x)
                        || (maxDistFromCenter >= center.y)
                        || (maxDistFromCenter >= center.z)) {
                    REPORT_ERROR(ERR_LOGIC_ERROR, "'maxDistFromCenter' is bigger than half of the signal's X, Y or Z dimensions");
                }
            }
        }
    }
};

template<typename T>
class AExtremaFinder {
public:
    // FIXME DS add cpu version
    AExtremaFinder() :
        m_isInit(false) {};

    virtual ~AExtremaFinder() {};

    void init(const ExtremaFinderSettings &settings, bool reuse);

    void find(T *data);

    HW& getHW() const { // FIXME DS remove once we use the new data-centric approach
        assert(m_isInit);
        return *m_settings.hw.at(0);
    }

    inline const ExtremaFinderSettings &getSettings() const {
        return m_settings;
    }

    inline const std::vector<T> &getValues() const {
        return m_values;
    }

    inline const std::vector<size_t> &getPositions() const {
        return m_positions;
    }


protected:
    virtual void check() const = 0;

    virtual void initMax() = 0;
    virtual void findMax(T *data) = 0;
    virtual bool canBeReusedMax(const ExtremaFinderSettings &s) const = 0;

    virtual void initMaxAroundCenter() = 0;
    virtual void findMaxAroundCenter(T *data) = 0;
    virtual bool canBeReusedMaxAroundCenter(const ExtremaFinderSettings &s) const = 0;

    inline std::vector<T> &getValues() {
        return m_values;
    }

    inline std::vector<size_t> &getPositions() {
        return m_positions;
    }

    inline constexpr bool isInitialized() const {
        return m_isInit;
    }

private:
    ExtremaFinderSettings m_settings;

    // results
    std::vector<T> m_values;
    std::vector<size_t> m_positions; // absolute, 0 based indices

    // flags
    bool m_isInit;

    bool canBeReused(const ExtremaFinderSettings &settings) const;
};

} /* namespace ExtremaFinder */

#endif /* LIBRARIES_RECONSTRUCTION_AFIND_EXTREMA_H_ */