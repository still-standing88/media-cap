#pragma once

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <sstream>
#include <string>
#include <vector>

namespace wstrutil
{
    /**
     * @brief Converts std::wstring to lower case.
     * @param str - std::wstring that needs to be converted.
     * @return Lower case input std::wstring.
     */
    static inline std::wstring to_lower(const std::wstring& str)
    {
        auto result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](wchar_t c) { return std::towlower(c); });
        return result;
    }

    /**
     * @brief Converts std::wstring to upper case.
     * @param str - std::wstring that needs to be converted.
     * @return Upper case input std::wstring.
     */
    static inline std::wstring to_upper(const std::wstring& str)
    {
        auto result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](wchar_t c) { return std::towupper(c); });
        return result;
    }

    /**
     * @brief Trims white spaces from the left side of std::wstring.
     * @param str - input std::wstring to remove white spaces from.
     */
    static inline void trim_left(std::wstring& str)
    {
        str.erase(str.begin(),
            std::find_if(str.begin(), str.end(),
                [](wchar_t ch) { return !std::iswspace(ch); }));
    }

    /**
     * @brief Trims white spaces from the right side of std::wstring.
     * @param str - input std::wstring to remove white spaces from.
     */
    static inline void trim_right(std::wstring& str)
    {
        str.erase(std::find_if(str.rbegin(), str.rend(),
            [](wchar_t ch) { return !std::iswspace(ch); }).base(), str.end());
    }

    /**
     * @brief Trims white spaces from both sides of std::wstring.
     * @param str - input std::wstring to remove white spaces from.
     */
    static inline void trim(std::wstring& str)
    {
        trim_left(str);
        trim_right(str);
    }

    /**
     * @brief Replaces first occurrence of target with replacement.
     * @param str - input std::wstring that will be modified.
     * @param target - substring that will be replaced with replacement.
     * @param replacement - substring that will replace target.
     * @return True if replacement was successful, false otherwise.
     */
    static inline bool replace_first(std::wstring& str,
        const std::wstring& target,
        const std::wstring& replacement)
    {
        const size_t start_pos = str.find(target);
        if (start_pos == std::wstring::npos)
        {
            return false;
        }

        str.replace(start_pos, target.length(), replacement);
        return true;
    }

    /**
     * @brief Replaces all occurrences of target with replacement.
     * @param str - input std::wstring that will be modified.
     * @param target - substring that will be replaced with replacement.
     * @param replacement - substring that will replace target.
     * @return True if any replacement was made, false otherwise.
     */
    static inline bool replace_all(std::wstring& str,
        const std::wstring& target,
        const std::wstring& replacement)
    {
        if (target.empty())
        {
            return false;
        }

        size_t start_pos = 0;
        const bool found_substring = str.find(target, start_pos) != std::wstring::npos;

        while ((start_pos = str.find(target, start_pos)) != std::wstring::npos)
        {
            str.replace(start_pos, target.length(), replacement);
            start_pos += replacement.length();
        }

        return found_substring;
    }

    /**
     * @brief Splits input std::wstring str according to input delim.
     * @param str - std::wstring that will be split.
     * @param delim - the delimiter.
     * @return std::vector<std::wstring> that contains all split tokens.
     */
    static inline std::vector<std::wstring> split(const std::wstring& str, wchar_t delim)
    {
        std::vector<std::wstring> tokens;
        std::wstringstream ss(str);
        std::wstring token;

        while (std::getline(ss, token, delim))
        {
            tokens.push_back(token);
        }

        if (str.empty() || str.back() == delim)
        {
            tokens.emplace_back();
        }

        return tokens;
    }

    /**
     * @brief Checks if std::wstring contains specified substring.
     * @param str - std::wstring to be checked.
     * @param substring - searched substring.
     * @return True if substring was found in str, false otherwise.
     */
    static inline bool contains(const std::wstring& str, const std::wstring& substring)
    {
        return str.find(substring) != std::wstring::npos;
    }

    /**
     * @brief Checks if std::wstring starts with specified prefix.
     * @param str - input std::wstring that will be checked.
     * @param prefix - searched prefix in str.
     * @return True if prefix was found, false otherwise.
     */
    static inline bool starts_with(const std::wstring& str, const std::wstring& prefix)
    {
        return str.rfind(prefix, 0) == 0;
    }

    /**
     * @brief Checks if std::wstring ends with specified suffix.
     * @param str - input std::wstring that will be checked.
     * @param suffix - searched suffix in str.
     * @return True if suffix was found, false otherwise.
     */
    static inline bool ends_with(const std::wstring& str, const std::wstring& suffix)
    {
        if (suffix.length() > str.length()) return false;
        return str.compare(str.length() - suffix.length(),
            suffix.length(), suffix) == 0;
    }
}