#pragma once

#include <string>
#include <cctype>
#include <string_view>

namespace fele
{
    namespace predicates
    {
        template<typename CharT = char>
        auto is(const CharT ch)
        {
            return [ch](CharT _ch)
            {
                return ch == _ch;
            };
        }

        template<typename CharT = char>
        constexpr auto is_space = [](CharT ch)
        {
            return static_cast<bool>(std::isspace(static_cast<CharT>(ch)));
        };
    }

    /// @brief Removes characters from the start of string that passed std::isspace
    /// @tparam CharT Character type. Defaults to char
    /// @param input The string
    /// @return Cleaned string
    template<typename CharT = char>
    std::basic_string<CharT> left_trim(std::basic_string<CharT> input);

    /// @brief Removes characters from the start of string that passed std::isspace
    /// @tparam CharT Character type. Defaults to char
    /// @tparam Predicate Function that takes a unsigned char as argument and returns a bool
    /// @param input The string
    /// @param pred Predicate that tells if a char is considered space
    /// @return Cleaned string
    template<typename CharT = char, typename Predicate = bool (*)(CharT)>
    std::basic_string<CharT> left_trim(std::basic_string<CharT> input, Predicate pred);

    /// @brief Removes characters from the end of string that passed std::isspace
    /// @tparam CharT Character type. Defaults to char
    /// @param input The string
    /// @return Cleaned string
    template<typename CharT = char>
    std::basic_string<CharT> right_trim(std::basic_string<CharT> input);

    /// @brief Removes characters from the end of string that passed std::isspace
    /// @tparam CharT Character type. Defaults to char
    /// @tparam Predicate Function that takes a unsigned char as argument and returns a bool
    /// @param input The string
    /// @param pred Predicate that tells if a char is considered space
    /// @return Cleaned string
    template<typename CharT = char, typename Predicate = bool (*)(CharT)>
    std::basic_string<CharT> right_trim(std::basic_string<CharT> input, Predicate pred);

    /// @brief Removes characters from both ends of string that passed predicate
    /// @tparam CharT Character type. Defaults to char
    /// @param input The string
    /// @return Cleaned string
    template<typename CharT = char>
    std::basic_string<CharT> trim(std::basic_string<CharT> input);

    /// @brief Removes characters from both ends of string that passed predicate
    /// @tparam CharT Character type. Defaults to char 
    /// @tparam Predicate Function that takes a unsigned char as argument and returns a bool
    /// @param input The string
    /// @param pred Predicate that tells if a char is considered space
    /// @return Cleaned string
    template<typename CharT = char, typename Predicate = bool (*)(CharT)>
    std::basic_string<CharT> trim(std::basic_string<CharT> input, Predicate pred);

    template <typename CharT>
    std::basic_string<CharT> left_trim(std::basic_string<CharT> input)
    {
        return left_trim(input, [](CharT ch)
        {
            return static_cast<bool>(std::isspace(static_cast<CharT>(ch)));
        });
    }

    template <typename CharT, typename Predicate>
    std::basic_string<CharT> left_trim(std::basic_string<CharT> input, Predicate pred)
    {
        size_t i = 0;
        if (input.size() == 0) return input;
        while(pred( static_cast<CharT>(input[i]) ) && i < input.size())
            i++;
        const auto size = input.size();
        for (size_t j = i; j < size; j++)
            input[j-i] = input[j];
        input.resize(input.size() - i);
        return input;
    }

    template <typename CharT>
    std::basic_string<CharT> right_trim(std::basic_string<CharT> input)
    {
        return right_trim(input, [](CharT ch)
        {
            return static_cast<bool>(std::isspace(static_cast<CharT>(ch)));
        });
    }

    template <typename CharT, typename Predicate>
    std::basic_string<CharT> right_trim(std::basic_string<CharT> input, Predicate pred)
    {
        if (input.size() == 0) input;
        auto i = input.size() - 1;
        while(pred(static_cast<CharT>(input[i])) && i >= 0)
            i--;
        input.resize(i + 1);
        return input;
    }

    template <typename CharT>
    std::basic_string<CharT> trim(std::basic_string<CharT> input)
    {
        return trim(input, predicates::is_space<CharT>);
    }

    template <typename CharT, typename Predicate>
    std::basic_string<CharT> trim(std::basic_string<CharT> input, Predicate pred)
    {
        return left_trim(right_trim(std::move(input), pred), pred);
    }
}
template<typename CharT = char>
std::basic_string<CharT> operator+(const std::basic_string<CharT>& str, std::basic_string_view<CharT> str_v);

template <typename CharT>
std::basic_string<CharT> operator+(const std::basic_string<CharT> &str, std::basic_string_view<CharT> str_v)
{
    auto s = str;
    s.reserve(s.size() + str_v.size());
    std::copy(str_v.begin(), str_v.end(), std::back_inserter(s));
    return s;
}

template<char space = ' '>
void left_trim(std::string& input)
{
    size_t i = 0;
    if (input.size() == 0) return;
    while(input[i] == space && i < input.size()) i++;
    input = input.substr(i);
}

template<char space = ' '>
void right_trim(std::string& input)
{
    if (input.size() == 0) return;
    auto i = input.size() - 1;
    while(input[i] == space && i >= 0) i--;
    input.resize(i + 1);
}

template<char lspace = ' ', char rspace = ' '>
void lr_trim(std::string& input)
{
    left_trim<lspace>(input);
    right_trim<rspace>(input);
}

template<char space = ' '>
std::string left_trim_cp(const std::string& input)
{
    size_t i = 0;
    if (input.size() == 0) return "";
    while(input[i] == space && i < input.size()) i++;
    return input.substr(i);
}

template<char space = ' '>
std::string right_trim_cp(const std::string& input)
{
    if (input.size() == 0) return "";
    size_t i = input.size() - 1;
    while(input[i] == space && i >= 0) i--;
    return input.substr(0, i + 1);
}

template<char lspace = ' ', char rspace = ' '>
std::string lr_trim_cp(const std::string& input)
{
    auto s = left_trim_cp<lspace>(input); // cp or not cp, it will make a copy of the string
    right_trim<rspace>(s); // we don't need to copy it again, so lets use the reference version
    return s;
}

template<typename CharT = char>
bool starts_with(std::basic_string_view<CharT> input, std::basic_string_view<CharT> what) {
    auto result = std::mismatch(input.begin(), input.end(), what.begin());
    return result.second == what.end();
}

template<typename CharT = char>
bool ends_with(std::basic_string_view<CharT> input, std::basic_string_view<CharT> what) {
    auto result = std::mismatch(input.rbegin(), input.rend(), what.rbegin());
    return result.second == what.rend();
}

template<typename CharT = char>
bool ichar_equals(CharT ch0, CharT ch1)
{
    return std::toupper(ch0) == std::toupper(ch1);
}

template<typename CharT = char>
bool istarts_with(std::basic_string_view<CharT> input, std::basic_string_view<CharT> what) {
    auto result = std::mismatch(input.begin(), input.end(), what.begin(), ichar_equals);
    return result.second == what.end();
}

template<typename CharT = char>
bool iends_with(std::basic_string_view<CharT> input, std::basic_string_view<CharT> what) {
    auto result = std::mismatch(input.rbegin(), input.rend(), what.rbegin(), ichar_equals);
    return result.second == what.rend();
}

template<typename T>
std::string join(const std::vector<T> vec, std::string sep = ",")
{
    std::string s;
    if (vec.empty()) return s;
    s += vec[0];

    for (size_t i = 1; i < vec.size(); i++)
    {
        if constexpr (std::is_same_v<std::string, std::decay_t<T>>)
            s.append(sep).append(vec[i]);
        else
            s.append(sep).append(std::to_string(vec[i]));
    }

    return s;
}