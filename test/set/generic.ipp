//
// immer - immutable data structures for C++
// Copyright (C) 2016, 2017 Juan Pedro Bolivar Puente
//
// This file is part of immer.
//
// immer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// immer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with immer.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef SET_T
#error "define the set template to use in SET_T"
#include <immer/set.hpp>
#define SET_T ::immer::set
#endif

#include "../util.hpp"

#include <immer/algorithm.hpp>

#include <catch.hpp>

#include <unordered_set>
#include <random>

template <typename T=unsigned>
auto make_generator()
{
    auto engine = std::default_random_engine{42};
    auto dist = std::uniform_int_distribution<T>{};
    return std::bind(dist, engine);
}

struct conflictor
{
    unsigned v1;
    unsigned v2;

    bool operator== (const conflictor& x) const
    { return v1 == x.v1 && v2 == x.v2; }
};

struct hash_conflictor
{
    std::size_t operator() (const conflictor& x) const
    { return x.v1; }
};

auto make_values_with_collisions(unsigned n)
{
    auto gen = make_generator();
    auto vals = std::vector<conflictor>{};
    auto vals_ = std::unordered_set<conflictor, hash_conflictor>{};
    generate_n(back_inserter(vals), n, [&] {
        auto newv = conflictor{};
        do {
            newv = { unsigned(gen() % (n / 2)), gen() };
        } while (!vals_.insert(newv).second);
        return newv;
    });
    return vals;
}

auto make_test_set(unsigned n)
{
    auto s = SET_T<unsigned>{};
    for (auto i = 0u; i < n; ++i)
        s = s.insert(i);
    return s;
}

auto make_test_set(const std::vector<conflictor>& vals)
{
    auto s = SET_T<conflictor, hash_conflictor>{};
    for (auto&& v : vals)
        s = s.insert(v);
    return s;
}

TEST_CASE("instantiation")
{
    SECTION("default")
    {
        auto v = SET_T<unsigned>{};
        CHECK(v.size() == 0u);
    }
}

TEST_CASE("basic insertion")
{
    auto v1 = SET_T<unsigned>{};
    CHECK(v1.count(42) == 0);

    auto v2 = v1.insert(42);
    CHECK(v1.count(42) == 0);
    CHECK(v2.count(42) == 1);

    auto v3 = v2.insert(42);
    CHECK(v1.count(42) == 0);
    CHECK(v2.count(42) == 1);
    CHECK(v3.count(42) == 1);
};

TEST_CASE("insert a lot")
{
    constexpr auto N = 666u;

    auto gen = make_generator();
    auto vals = std::vector<unsigned>{};
    generate_n(back_inserter(vals), N, gen);
    auto s = SET_T<unsigned>{};

    for (auto i = 0u; i < N; ++i) {
        s = s.insert(vals[i]);
        CHECK(s.size() == i + 1);
        for (auto j : test_irange(0u, i+1))
            CHECK(s.count(vals[j]) == 1);
        for (auto j : test_irange(i + 1u, N))
            CHECK(s.count(vals[j]) == 0);
    }
}

TEST_CASE("insert conflicts")
{
    constexpr auto N = 666u;
    auto vals = make_values_with_collisions(N);
    auto s = SET_T<conflictor, hash_conflictor>{};
    for (auto i = 0u; i < N; ++i) {
        s = s.insert(vals[i]);
        CHECK(s.size() == i + 1);
        for (auto j : test_irange(0u, i+1))
            CHECK(s.count(vals[j]) == 1);
        for (auto j : test_irange(i + 1u, N))
            CHECK(s.count(vals[j]) == 0);
    }
}

TEST_CASE("erase a lot")
{
    constexpr auto N = 666u;
    auto gen = make_generator();
    auto vals = std::vector<unsigned>{};
    generate_n(back_inserter(vals), N, gen);

    auto s = SET_T<unsigned>{};
    for (auto i = 0u; i < N; ++i)
        s = s.insert(vals[i]);

    for (auto i = 0u; i < N; ++i) {
        s = s.erase(vals[i]);
        CHECK(s.size() == N - i - 1);
        for (auto j : test_irange(0u, i+1))
            CHECK(s.count(vals[j]) == 0);
        for (auto j : test_irange(i + 1u, N))
            CHECK(s.count(vals[j]) == 1);
    }
}

TEST_CASE("erase conflicts")
{
    constexpr auto N = 666u;
    auto vals = make_values_with_collisions(N);
    auto s = SET_T<conflictor, hash_conflictor>{};
    for (auto i = 0u; i < N; ++i)
        s = s.insert(vals[i]);

    for (auto i = 0u; i < N; ++i) {
        s = s.erase(vals[i]);
        CHECK(s.size() == N - i - 1);
        for (auto j : test_irange(0u, i+1))
            CHECK(s.count(vals[j]) == 0);
        for (auto j : test_irange(i + 1u, N))
            CHECK(s.count(vals[j]) == 1);
    }
}

TEST_CASE("accumulate")
{
    const auto n = 666u;
    auto v = make_test_set(n);

    auto expected_n =
        [] (auto n) {
            return n * (n - 1) / 2;
        };

    SECTION("sum collection")
    {
        auto sum = immer::accumulate(v, 0u);
        CHECK(sum == expected_n(v.size()));
    }

    SECTION("sum collisions") {
        auto vals = make_values_with_collisions(n);
        auto s = make_test_set(vals);
        auto acc = [] (unsigned r, conflictor x) {
            return r + x.v1 + x.v2;
        };
        auto sum1 = std::accumulate(vals.begin(), vals.end(), 0, acc);
        auto sum2 = immer::accumulate(s, 0u, acc);
        CHECK(sum1 == sum2);
    }
}

TEST_CASE("iterator")
{
    const auto N = 666u;
    auto v = make_test_set(N);

    SECTION("empty set")
    {
        auto s = SET_T<unsigned>{};
        CHECK(s.begin() == s.end());
    }

    SECTION("works with range loop")
    {
        auto seen = std::unordered_set<unsigned>{};
        for (const auto& x : v)
            CHECK(seen.insert(x).second);
        CHECK(seen.size() == v.size());
    }

    SECTION("works with standard algorithms")
    {
        auto s = std::vector<unsigned>(N);
        std::iota(s.begin(), s.end(), 0u);
        std::equal(v.begin(), v.end(), s.begin(), s.end());
    }

    SECTION("iterator and collisions")
    {
        auto vals = make_values_with_collisions(N);
        auto s = make_test_set(vals);
        auto seen = std::unordered_set<conflictor, hash_conflictor>{};
        for (const auto& x : s)
            CHECK(seen.insert(x).second);
        CHECK(seen.size() == s.size());
    }
}