#pragma once

#include <string>
#include <outcome.hpp>
#include <magic_enum.hpp>

// TODO: Document it

template<typename ECode, typename Diag = std::string>
struct Error
{
    Error(ECode ecode) : ecode(ecode) {}
    Error(ECode ecode, Diag diag) : ecode(ecode), diag(std::move(diag)) {}
    operator ECode() { return ecode; }
    Error with_message(Diag add_diag) const { return Error{ecode, std::move(add_diag)}; }
    ECode ecode;
    Diag diag;
    std::string to_string() const
    {
        return std::string{magic_enum::enum_name(ecode)} + "{" + diag + "}";
    }
};

namespace outcome = OUTCOME_V2_NAMESPACE;

template<typename R, typename E>
using Result = outcome::result<R, Error<E>>;

template<typename R, typename E>
bool is_error(const Result<R, E>& result, E ecode)
{
    return result.has_error() && result.error().ecode == ecode;
}