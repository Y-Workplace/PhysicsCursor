#include "VariantValue.hpp"

#include <format>

CVariantValue::CVariantValue(const char* name, const char* def, const char* desc, std::unordered_map<std::string, int> map) :
    m_config(makeShared<CStringValue>(name, desc, std::string{def})), m_map(std::move(map)) {
    // Defaults must be valid without forcing a global config reload.
    m_cached = m_map.at(def);
}

std::optional<int> CVariantValue::lookup(const std::string& str) const {
    auto it = m_map.find(str);
    if (it == m_map.end())
        return {};

    return it->second;
}

std::string CVariantValue::formatVariants() const {
    std::string variants = "";

    for (auto& [variant, _] : m_map)
        variants += std::format("{}`{}`", variants.empty() ? "" : ", ", variant);

    return variants;
}

WP<IValue> CVariantValue::internal() {
    return m_config;
}

std::optional<std::string> CVariantValue::reload() {
    const auto& str    = m_config->value();
    auto        result = lookup(str);

    if (!result)
        return std::format("invalid variant `{}`, must be one of {}", str, formatVariants());

    m_cached = *result;
    return {};
}
