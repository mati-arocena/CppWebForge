#pragma once

#include <string>
#include <filesystem>
#include <map>
#include <memory>
#include <variant>
#include <vector>
#include <stdexcept>

namespace cppwebforge {

class TemplateError : public std::runtime_error {
public:
    explicit TemplateError(const std::string& message) : std::runtime_error(message) {}
};

class TemplateRenderer {
public:
    struct DataValue;
    using DataMap = std::map<std::string, DataValue>;
    using DataArray = std::vector<DataValue>;
    
    struct DataValue : std::variant<std::string, int, double, bool, DataMap, DataArray> {
        using variant::variant;
    };

    TemplateRenderer();
    ~TemplateRenderer();
    
    TemplateRenderer(const TemplateRenderer&) = delete;
    TemplateRenderer& operator=(const TemplateRenderer&) = delete;
    
    TemplateRenderer(TemplateRenderer&&) noexcept;
    TemplateRenderer& operator=(TemplateRenderer&&) noexcept;

    void renderTemplate(const std::filesystem::path& templatePath,
                       const std::filesystem::path& outputDir,
                       const std::string& outputFilename,
                       const DataMap& data);

private:
    class TemplateRendererImpl;
    std::unique_ptr<TemplateRendererImpl> impl_;
};

} // namespace cppwebforge
