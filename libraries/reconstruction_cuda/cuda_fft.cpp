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

#include "cuda_fft.h"
#include <cufft.h>
#include "cuda_utils.h"

template<typename T>
void CudaFFT<T>::init(const FFTSettingsNew<T> &settings) {
    m_settings = settings;
    m_plan = createPlan(m_settings);

    if (m_settings.isInPlace()) {
        REPORT_ERROR(ERR_NOT_IMPLEMENTED, "In place transformations are not yet available. "
                "Stay tuned!");
    } else {
        gpuErrchk(cudaMalloc(&m_d_FD, m_settings.fBytesBatch()));
        gpuErrchk(cudaMalloc(&m_d_SD, m_settings.sBytesBatch()));
    }

    m_isInit = true;
}


template<typename T>
std::complex<T>* CudaFFT<T>::fft(const T *h_in,
        std::complex<T> *h_out) {
    auto isReady = (m_isInit);
    if ( ! isReady) {
        REPORT_ERROR(ERR_LOGICAL_ERROR, "Not ready to perform Fourier Transform. "
                "Call init function first");
    }

    // process signals in batches
    for (size_t offset = 0; offset < m_settings.sDim().n(); offset += m_settings.batch()) {
        // how many signals to process
        size_t toProcess = std::min(m_settings.batch(), m_settings.sDim().n() - offset);

        // copy memory
        gpuErrchk(cudaMemcpy(
                m_d_SD,
                h_in + offset * m_settings.sDim().xyz(),
                toProcess * m_settings.sBytesSingle(),
                    cudaMemcpyHostToDevice));

        fft(m_plan, m_d_SD, m_d_FD);

        // copy data back
        gpuErrchk(cudaMemcpy(
                h_out + offset * m_settings.fDim().xyz(),
                m_d_FD,
                toProcess * m_settings.fBytesSingle(),
                cudaMemcpyDeviceToHost));
    }
    return h_out;
}


template<typename T>
void CudaFFT<T>::setDefault() {
    m_settings = FFTSettingsNew<T>(0);
    m_isInit = false;
    m_d_SD = nullptr;
    m_d_FD = nullptr;
}

template<typename T>
void CudaFFT<T>::release() {
    gpuErrchk(cudaFree(m_d_SD));
    gpuErrchk(cudaFree(m_d_FD));
    setDefault();
}

template<typename T>
std::complex<T>* CudaFFT<T>::fft(cufftHandle plan, const T *d_in,
        std::complex<T> *d_out) {
    if (std::is_same<T, float>::value) {
        gpuErrchkFFT(cufftExecR2C(plan, (cufftReal*)d_in, (cufftComplex*)d_out));
    } else if (std::is_same<T, double>::value){
        gpuErrchkFFT(cufftExecD2Z(plan, (cufftDoubleReal*)d_in, (cufftDoubleComplex*)d_out));
    } else {
        REPORT_ERROR(ERR_TYPE_INCORRECT, "Not implemented");
    }
    return d_out;
}

template<typename T>
std::complex<T>* CudaFFT<T>::fft(cufftHandle plan, T *d_inOut) {
    return fft(plan, d_inOut, (std::complex<T>*) d_inOut);
}

template<typename T>
cufftHandle CudaFFT<T>::createPlan(const FFTSettingsNew<T> &settings) {
    std::array<int, 3> n;
    int idist;
    int odist;
    cufftHandle plan;
    cufftType type;
    if (settings.isForward()) {
        n = {(int)settings.sDim().z(), (int)settings.sDim().y(), (int)settings.sDim().x()};
        idist = settings.sDim().xyz();
        odist = settings.fDim().xyz();
        type = CUFFT_R2C;
    } else {
        n ={(int)settings.fDim().z(), (int)settings.fDim().y(), (int)settings.fDim().x()};
        idist = settings.fDim().xyz();
        odist = settings.sDim().xyz();
        type = CUFFT_C2R;
    }
    int rank = 3;
    if (settings.sDim().z() == 1) rank--;
    if (settings.sDim().y() == 1) rank--;

    int offset = 3 - rank;

    gpuErrchkFFT(cufftPlanMany(&plan, rank, &n[offset], nullptr,
        1, idist, nullptr, 1, odist, type, settings.batch()));
    return plan;
}

// explicit instantiation
template class CudaFFT<float>;
