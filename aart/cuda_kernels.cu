#include <cuda.h>
#include <cuda_runtime.h>
#include <opencv2/core/cuda.hpp>

#include "Colors.h"
#include "cuda_kernels.h"

template <typename T>
using matptr_t = cv::cuda::PtrStepSz<T>;

__device__ inline float CIE76_compare_(const lab_t<float>* x, const lab_t<float>* y)
{
    return (x->l - y->l) * (x->l - y->l) + (x->a - y->a) * (x->a - y->a) + (x->b - y->b) * (x->b - y->b);
}

__global__ void similar2_CIE76_compare_(const matptr_t<lab_t<float>> picture, const matptr_t<lab_t<float>> colormap, similar_t* similar)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x <= picture.cols - 1 && y <= picture.rows - 1 && y >= 0 && x >= 0)
    {
        const auto goal = picture(y, x);
        const auto start_color = colormap(0, 0);
        auto delta1 = CIE76_compare_(&goal, &start_color);
        auto delta2 = delta1;
        auto index1 = 0;
        auto index2 = index1;

        for (int i = 1; i < colormap.cols; ++i)
        {
            const auto color = colormap(0, i);
            const auto delta = CIE76_compare_(&goal, &color);

            if (delta < delta1) {
                delta2 = delta1;
                delta1 = delta;

                index2 = index1;
                index1 = i;
            }
            else if (delta < delta2) {
                delta2 = delta;

                index2 = i;
            }
        }

        similar[y * picture.cols + x] = similar_t{
                 delta1,  delta2,
                 index1,  index2
        };
    }
}

__global__ void copy_symbols_(matptr_t<rgb_t<uint8_t>> picture,
    const matptr_t<rgb_t<uint8_t>> charmap,
    const similar_t* colors, int w, int h, int cellW, int cellH, int nColors, int nChars)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x <= w - 1 && y <= h - 1 && y >= 0 && x >= 0)
    {
        const int art_x = x * cellW;
        const int art_y = y * cellH;

        const auto similar = colors[y * w + x];

        /*const int char_pos = similar.fg_delta == 0 ?
            nChars - 1 :
            similar.bg_delta / similar.fg_delta * (nChars - 1);*/

        const int char_pos = __fdividef(fmaf(similar.bg_delta, nChars, -similar.bg_delta), similar.fg_delta);

        const auto cell_x = char_pos * cellW;
        const auto cell_y = (similar.bg_index * nColors + similar.fg_index) * cellH;

        for (int yPos = 0; yPos < cellH; ++yPos)
        {
            for (int xPos = 0; xPos < cellW; ++xPos)
            {
                picture(art_y + yPos, art_x + xPos) = charmap(cell_y + yPos, cell_x + xPos);

            }
        }
    }
}

__global__ void divide_(matptr_t<lab_t<float>> mat, float val)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x <= mat.cols - 1 && y <= mat.rows - 1 && y >= 0 && x >= 0)
    {
        auto color = mat(y, x);
        color.l = __fdividef(color.l, val);
        color.a = __fdividef(color.a, val);
        color.b = __fdividef(color.b, val);
        mat(y, x) = color;
    }
}



[[nodiscard]] auto similar2_CIE76_compare(const cv::cuda::GpuMat& picture, const cv::cuda::GpuMat& colormap) -> similarptr_t
{
    dim3 cthreads{ 16, 16 };
    dim3 cblocks{
            static_cast<unsigned>(std::ceil(picture.size().width /
                static_cast<double>(cthreads.x))),
            static_cast<unsigned>(std::ceil(picture.size().height /
                static_cast<double>(cthreads.y)))
    };

    similar_t* gpu_similar;
    cudaMalloc(&gpu_similar, sizeof(similar_t)* picture.rows* picture.cols);
    similar2_CIE76_compare_ << <cblocks, cthreads >> > (picture, colormap, gpu_similar);

    return similarptr_t{ gpu_similar, [](similar_t* similar) noexcept { cudaFree(similar); } };
}

[[nodiscard]] auto copy_symbols(cv::cuda::GpuMat& art, const cv::cuda::GpuMat& charmap,
    const similarptr_t colors, int w, int h, int cellW, int cellH, int nColors, int nChars) -> void
{
    dim3 cthreads{ 16, 16 };
    dim3 cblocks{
        static_cast<unsigned>(std::ceil(w /
            static_cast<double>(cthreads.x))),
        static_cast<unsigned>(std::ceil(h /
            static_cast<double>(cthreads.y)))
    };

    copy_symbols_ << <cblocks, cthreads >> > (art, charmap, colors.get(), w, h, cellW, cellH, nColors, nChars);
    auto error = cudaGetLastError();
}

auto cuda_divide(cv::cuda::GpuMat& mat, float x) -> void
{
    dim3 cthreads{ 16, 16 };
    dim3 cblocks{
        static_cast<unsigned>(std::ceil(mat.size().width /
            static_cast<double>(cthreads.x))),
        static_cast<unsigned>(std::ceil(mat.size().height /
            static_cast<double>(cthreads.y)))
    };

    divide_ << <cblocks, cthreads >> > (mat, x);
}
