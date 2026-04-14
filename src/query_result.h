#pragma once

#include <pqxx/pqxx>

#include <optional>
#include <string>
#include <string_view>
#include <cstdint>
#include <stdexcept>

namespace NTPCC {

class QueryResult {
public:
    QueryResult() = default;
    explicit QueryResult(pqxx::result result)
        : Result_(std::move(result))
        , CurrentRow_(0)
    {}

    bool TryNextRow() {
        if (CurrentRow_ < Result_.size()) {
            Row_.emplace(Result_[CurrentRow_]);
            ++CurrentRow_;
            return true;
        }
        Row_.reset();
        return false;
    }

    size_t GetRowsCount() const {
        return Result_.size();
    }

    bool IsEmpty() const {
        return Result_.empty();
    }

    //--- typed column access (by column name) ---

    int32_t GetInt32(std::string_view col) const {
        return (*Row_)[std::string(col)].as<int32_t>();
    }

    int64_t GetInt64(std::string_view col) const {
        return (*Row_)[std::string(col)].as<int64_t>();
    }

    uint64_t GetUint64(std::string_view col) const {
        return (*Row_)[std::string(col)].as<uint64_t>();
    }

    double GetDouble(std::string_view col) const {
        return (*Row_)[std::string(col)].as<double>();
    }

    std::string GetString(std::string_view col) const {
        return (*Row_)[std::string(col)].as<std::string>();
    }

    std::optional<int32_t> GetOptionalInt32(std::string_view col) const {
        auto field = (*Row_)[std::string(col)];
        if (field.is_null()) return std::nullopt;
        return field.as<int32_t>();
    }

    std::optional<int64_t> GetOptionalInt64(std::string_view col) const {
        auto field = (*Row_)[std::string(col)];
        if (field.is_null()) return std::nullopt;
        return field.as<int64_t>();
    }

    std::optional<uint64_t> GetOptionalUint64(std::string_view col) const {
        auto field = (*Row_)[std::string(col)];
        if (field.is_null()) return std::nullopt;
        return field.as<uint64_t>();
    }

    std::optional<double> GetOptionalDouble(std::string_view col) const {
        auto field = (*Row_)[std::string(col)];
        if (field.is_null()) return std::nullopt;
        return field.as<double>();
    }

    std::optional<std::string> GetOptionalString(std::string_view col) const {
        auto field = (*Row_)[std::string(col)];
        if (field.is_null()) return std::nullopt;
        return field.as<std::string>();
    }

    //--- typed column access (by column index) ---

    int32_t GetInt32(size_t col) const {
        return (*Row_)[static_cast<pqxx::row::size_type>(col)].as<int32_t>();
    }

    int64_t GetInt64(size_t col) const {
        return (*Row_)[static_cast<pqxx::row::size_type>(col)].as<int64_t>();
    }

    double GetDouble(size_t col) const {
        return (*Row_)[static_cast<pqxx::row::size_type>(col)].as<double>();
    }

    std::string GetString(size_t col) const {
        return (*Row_)[static_cast<pqxx::row::size_type>(col)].as<std::string>();
    }

    const pqxx::result& GetRawResult() const { return Result_; }

private:
    pqxx::result Result_;
    std::optional<pqxx::row> Row_;
    size_t CurrentRow_ = 0;
};

} // namespace NTPCC
