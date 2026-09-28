#include "../Source/dsp/NamWrapper.h"
#include <iostream>
#include <chrono>
int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Usage: CassianModelCheck path-to-capture.nam\n"; return 2; }
    try
    {
        NamWrapper model {juce::File(juce::String::fromUTF8(argv[1]))};
        const double rate = model.expectedRate() > 0 ? model.expectedRate() : 48000;
        model.prepare(rate, 128);
        float peak = 0;
        double squareSum = 0;
        std::array<float, 128> samples;
        const auto start = std::chrono::steady_clock::now();
        for (int block = 0; block < 375; ++block)
        {
            for (size_t i = 0; i < samples.size(); ++i)
                samples[i] = 0.05f * std::sin(static_cast<float>((block * 128 + i) * 2.0 * 3.141592653589793 * 220 / rate));
            model.process(samples.data(), static_cast<int>(samples.size()));
            for (const auto sample : samples)
            {
                if (!std::isfinite(sample)) throw std::runtime_error("Model produced non-finite audio");
                peak = std::max(peak, std::abs(sample)); squareSum += sample * sample;
            }
        }
        const auto milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Loaded: " << argv[1] << "\nSample rate: " << rate << "\nOutput peak: " << peak
            << "\nOutput RMS: " << std::sqrt(squareSum / 48000) << "\nRender milliseconds for 48000 samples: " << milliseconds << std::endl;
        if (peak == 0) throw std::runtime_error("Model produced only silence");
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "Load/render failed: " << error.what() << std::endl; return 1; }
}
