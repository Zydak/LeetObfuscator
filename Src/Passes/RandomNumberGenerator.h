#pragma once

#include <random>
#include <memory>
#include <type_traits>
#include <cstdint>

namespace LeetObfuscator
{
    class RandomNumberGenerator
    {
    public:
        RandomNumberGenerator(uint64_t seed)
            : m_Seed(seed), m_RandomEngine(seed)
        {

        }

        template <typename T>
        T DrawRange(const T& min, const T& max)
        {
            if constexpr (std::is_integral_v<T>)
            {
                std::uniform_int_distribution<T> dist(min, max);
                return dist(m_RandomEngine);
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                std::uniform_real_distribution<T> dist(min, max);
                return dist(m_RandomEngine);
            }
            else
            {
                static_assert(std::is_arithmetic_v<T>, "Unsupported random type");
            }
        }

        static void CreateGlobalRandomNumberGenerator(uint64_t seed)
        {
            s_GlobalRandomNumberGen = std::make_shared<RandomNumberGenerator>(seed);
        }

        static std::shared_ptr<RandomNumberGenerator> GetGlobalRandomNumberGenerator()
        {
            return s_GlobalRandomNumberGen;
        }

        inline uint64_t GetSeed() const { return m_Seed; } 
    private:

        static inline std::shared_ptr<RandomNumberGenerator> s_GlobalRandomNumberGen;
        uint64_t m_Seed;
        std::mt19937 m_RandomEngine;
    };
}