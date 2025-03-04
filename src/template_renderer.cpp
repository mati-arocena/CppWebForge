#include "template_renderer.h"
#include <fstream>
#include <system_error>
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

namespace cppwebforge {

class TemplateRenderer::TemplateRendererImpl {
public:
    TemplateRendererImpl() {
        env_.set_trim_blocks(true);
        env_.set_lstrip_blocks(true);
    }

    nlohmann::json convertToJson(const DataValue& value) {
        if (value.valueless_by_exception()) {
            throw TemplateError("Invalid variant value");
        }

        return std::visit([this](const auto& value) -> nlohmann::json {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::string> ||
                         std::is_same_v<T, int> ||
                         std::is_same_v<T, double> ||
                         std::is_same_v<T, bool>) {
                return value;
            } 
            else if constexpr (std::is_same_v<T, DataMap>) {
                nlohmann::json obj;
                for (const auto& [key, val] : value) {
                    obj[key] = convertToJson(val);
                }
                return obj;
            } 
            else if constexpr (std::is_same_v<T, DataArray>) {
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& element : value) {
                    arr.push_back(convertToJson(element));
                }
                return arr;
            }
        }, value);
    }

    void renderTemplate(const std::filesystem::path& templatePath,
                       const std::filesystem::path& outputDir,
                       const std::string& outputFilename,
                       const DataMap& data) {
        if (!std::filesystem::exists(templatePath)) {
            throw TemplateError("Template file does not exist: " + templatePath.string());
        }

        std::error_code errorCode;
        if (!std::filesystem::exists(outputDir)) {
            if (!std::filesystem::create_directories(outputDir, errorCode)) {
                throw TemplateError("Failed to create output directory: " + errorCode.message());
            }
        }

        nlohmann::json jsonData;
        for (const auto& [key, value] : data) {
            jsonData[key] = convertToJson(value);
        }

        auto outputPath = outputDir / outputFilename;

        try {
            auto result = env_.render_file(templatePath.string(), jsonData);
            
            std::ofstream outputFile(outputPath);
            if (!outputFile.is_open()) {
                throw TemplateError("Failed to open output file for writing: " + outputPath.string());
            }
            
            outputFile << result;
            outputFile.close();
            
        } catch (const inja::InjaError& e) {
            throw TemplateError("Template rendering error: " + std::string(e.what()));
        }
    }

private:
    inja::Environment env_;
};

TemplateRenderer::TemplateRenderer() : impl_(std::make_unique<TemplateRendererImpl>()) {}
TemplateRenderer::~TemplateRenderer() = default;

TemplateRenderer::TemplateRenderer(TemplateRenderer&&) noexcept = default;
TemplateRenderer& TemplateRenderer::operator=(TemplateRenderer&&) noexcept = default;

void TemplateRenderer::renderTemplate(const std::filesystem::path& templatePath,
                                    const std::filesystem::path& outputDir,
                                    const std::string& outputFilename,
                                    const DataMap& data) {
    impl_->renderTemplate(templatePath, outputDir, outputFilename, data);
}

} // namespace cppwebforge
