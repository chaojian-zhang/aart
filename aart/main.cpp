#include <numeric>
#include <opencv2/opencv.hpp>

// Declare generic type for rgb color space

template <typename T>
struct rgb_t
{
	using value_type = T;

	constexpr rgb_t(T r, T g, T b) noexcept :
		r{ r },
		g{ g },
		b{ b }
	{}

	T r{};
	T g{};
	T b{};
};

namespace cv {
	template<typename T>
	struct DataType<rgb_t<T>>
	{
		using value_type   = rgb_t<T>;
		using work_type    = typename DataType<T>::work_type;
		using channel_type = T;

		enum {
			generic_type = 0,
			channels     = 3,
			fmt          = traits::SafeFmt<channel_type>::fmt + ((channels - 1) << 8)
		};

		using vec_type = Vec<channel_type, channels>;
	};

	namespace traits {
		template<typename T>
		struct Depth<rgb_t<T>>
		{
			enum { value = Depth<T>::value };
		};

		template<typename T>
		struct Type<rgb_t<T>>
		{
			enum { value = CV_MAKETYPE(Depth<T>::value, 3) };
		};
	}
}

// Declare generic type for lab color space

template <typename T>
struct lab_t
{
	using value_type = T;

	constexpr lab_t(T l, T a, T b) noexcept :
		l{ l },
		a{ a },
		b{ b }
	{}

	T l{};
	T a{};
	T b{};
};

namespace cv {
	template<typename T>
	struct DataType<lab_t<T>>
	{
		using value_type = lab_t<T>;
		using work_type = typename DataType<T>::work_type;
		using channel_type = T;

		enum {
			generic_type = 0,
			channels = 3,
			fmt = traits::SafeFmt<channel_type>::fmt + ((channels - 1) << 8)
		};

		using vec_type = Vec<channel_type, channels>;
	};

	namespace traits {
		template<typename T>
		struct Depth<lab_t<T>>
		{
			enum { value = Depth<T>::value };
		};

		template<typename T>
		struct Type<lab_t<T>>
		{
			enum { value = CV_MAKETYPE(Depth<T>::value, 3) };
		};
	}
}

[[nodiscard]] auto normalize(cv::Mat& img) noexcept -> void
{
	img /= 255.0;
}

[[nodiscard]] auto bgr2lab(const cv::Mat& img) noexcept -> cv::Mat
{
	cv::Mat result;
	img.convertTo(result, CV_32F);
	normalize(result);
	cv::cvtColor(std::move(result), result, cv::COLOR_BGR2Lab);
	return result;
}

template <typename T>
[[nodiscard]] constexpr auto CIE76_distance(const lab_t<T>& x, const lab_t<T>& y) noexcept -> T
{
	return sqrt(pow(x.l - y.l, 2) + pow(x.a - y.a, 2) + pow(x.b - y.b, 2));
}

template <typename T, typename F>
[[nodiscard]] auto similar(const T& goal, const cv::Mat& palette, F distance) noexcept -> T
{
	return std::reduce(palette.begin<T>() + 1, palette.end<T>(), *palette.begin<T>(),
		[&goal, distance] (auto&& prev, auto&& curr) noexcept
		{
			const auto prevD = distance(goal, std::forward<decltype(prev)>(prev));
			const auto currD = distance(goal, std::forward<decltype(curr)>(curr));
			return currD < prevD ? curr : prev;
		}
	);

	/*auto dmin = distance(goal, *palette.begin<T>());
	auto result = *palette.begin<T>();

	std::for_each(palette.begin<T>() + 1, palette.end<T>(), [&](auto color) noexcept {
		const auto dcolor = distance(goal, color);
		if (dcolor < dmin)
		{
			dmin = dcolor;
			result = color;
		}
	});

	return result;*/
}

template <typename T, typename F>
[[nodiscard]] auto similar2(const T& goal, const cv::Mat& palette, F distance) noexcept -> std::tuple<cv::MatConstIterator_<T>, cv::MatConstIterator_<T>>
{
	auto dmin1 = distance(goal, *palette.begin<T>());
	auto dmin2 = dmin1;
	auto c1 = palette.begin<T>();
	auto c2 = c1;

	
	for (auto c = palette.begin<T>() + 1; c != palette.end<T>(); ++c)
	{
		const auto delta = distance(goal, *c);

		if (delta < dmin1) {
			dmin2 = dmin1;
			dmin1 = delta;

			c2 = c1;
			c1 = c;
		}
		else if (delta < dmin2) {
			dmin2 = delta;

			c2 = c;
		}
	}

	return std::make_tuple(c1, c2);
}

class Charmap
{
public:
	Charmap(cv::Mat charmap, cv::Mat colormap, std::string chars) :
		m_charmap{ std::move(charmap) },
		m_colormap{ bgr2lab(std::move(colormap)) },
		m_chars{ std::move(chars) }
	{}

	[[nodiscard]] inline auto cellW() const noexcept
	{
		return m_cellw;
	}

	[[nodiscard]] inline auto cellH() const noexcept
	{
		return m_cellh;
	}

	[[nodiscard]] inline auto size() const noexcept
	{
		return m_ncells;
	}

	[[nodiscard]] inline auto type() const noexcept
	{
		return m_charmap.type();
	}

	template <typename T, typename F>
	[[nodiscard]] auto getCell(const T& color, F distance) const noexcept -> cv::Mat
	{
		// Get best colors and calculate its palette index
		const auto [bg_color, fg_color] = similar2(color, m_colormap, distance);
		const auto [bg_pos, fg_pos] = calc_pos(bg_color, fg_color);

		const auto bg_delta = distance(color, *bg_color);
		const auto fg_delta = distance(color, *fg_color);

		// Calculate character index
		const int char_pos = fg_delta == 0 ?
			m_nchars - 1 :
			bg_delta / fg_delta * (m_nchars - 1);

		// Calculate celll position in charmap
		const auto cell_x = char_pos * m_cellw;
		const auto cell_y = (bg_pos * m_ncolors + fg_pos) * m_cellh;

		return m_charmap(cv::Rect{ cell_x, cell_y, m_cellw, m_cellh });
	}

private:
	cv::Mat m_charmap;
	cv::Mat m_colormap;
	const std::string m_chars;

	const int m_nchars  = m_chars.length();
	const int m_ncolors = m_colormap.size().width;

	const int m_cellw = m_charmap.size().width / m_nchars;
	const int m_cellh = m_charmap.size().height / (m_ncolors * m_ncolors);
	const int m_ncells = m_nchars * m_ncolors * m_ncolors;

	template <typename T>
	[[nodiscard]] inline auto calc_pos(
		const cv::MatConstIterator_<T>& bg_color,
		const cv::MatConstIterator_<T>& fg_color) const noexcept
		-> std::tuple<int, int>
	{
		const auto start_color = m_colormap.begin<T>();
		return std::make_tuple(bg_color - start_color, fg_color - start_color);
	}
};

int main()
{
	const auto charmap = Charmap{
		cv::imread("charmap.png", cv::IMREAD_COLOR),
		cv::imread("colormap.png", cv::IMREAD_COLOR),
		" .:-=+*#%@"
	};

	auto pic = cv::imread("test.jpg", cv::IMREAD_COLOR);
	cv::resize(pic, pic, {}, 1.0, (double)charmap.cellW() / charmap.cellH(), cv::INTER_LINEAR);
	pic = bgr2lab(pic);

	const auto picw = pic.size().width;
	const auto pich = pic.size().height;
	const auto cellw = charmap.cellW();
	const auto cellh = charmap.cellH();
	auto art = cv::Mat(pich * cellh, picw * cellw, charmap.type());

	pic.forEach<lab_t<float>>([&art, &charmap](auto p, const int* pos) noexcept {
		const auto y = pos[0];
		const auto x = pos[1];
		
		const auto cellw = charmap.cellW();
		const auto cellh = charmap.cellH();

		auto cell = charmap.getCell(p, CIE76_distance<float>);
		const auto roi = cv::Rect{ x * cellw, y * cellh, cellw, cellh };
		cell.copyTo(art(roi));
	});

	cv::cvtColor(pic, pic, cv::COLOR_Lab2BGR);

	cv::imshow("Result", art);
	cv::imwrite("result.png", art);
	cv::waitKey(0);
}
