#include "../include/template_renderer.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace cppwebforge {

class TemplateRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories("test_templates");
        std::filesystem::create_directories("test_output");
    }

    void TearDown() override {
        std::filesystem::remove_all("test_templates");
        std::filesystem::remove_all("test_output");
    }

    void createTemplateFile(const std::string& filename, const std::string& content) {
        std::ofstream file("test_templates/" + filename);
        file << content;
        file.close();
    }

    std::string readOutputFile(const std::string& filename) {
        std::ifstream file("test_output/" + filename);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        return content;
    }

    TemplateRenderer renderer;
};

TEST_F(TemplateRendererTest, BasicTemplateRendering) {
    createTemplateFile("basic.txt", "Hello {{ name }}!");

    TemplateRenderer::DataMap data;
    data["name"] = std::string("World");

    renderer.renderTemplate("test_templates/basic.txt", "test_output", "result.txt", data);

    EXPECT_EQ(readOutputFile("result.txt"), "Hello World!");
}

TEST_F(TemplateRendererTest, AllDataTypes) {
    createTemplateFile("types.txt", 
        "String: {{ text }}\n"
        "Number: {{ number }}\n"
        "Decimal: {{ decimal }}\n"
        "Flag: {{ flag }}"
    );

    TemplateRenderer::DataMap data;
    data["text"] = std::string("Hello");
    data["number"] = 42;
    data["decimal"] = 3.14;
    data["flag"] = true;

    renderer.renderTemplate("test_templates/types.txt", "test_output", "types.txt", data);

    std::string expected = 
        "String: Hello\n"
        "Number: 42\n"
        "Decimal: 3.14\n"
        "Flag: true";
    
    EXPECT_EQ(readOutputFile("types.txt"), expected);
}

TEST_F(TemplateRendererTest, ControlStructures) {
    createTemplateFile("control.txt",
        "{% if show_greeting %}Hello{% endif %}"
        "{% if not show_greeting %}Goodbye{% endif %}"
    );

    TemplateRenderer::DataMap data;
    data["show_greeting"] = true;

    renderer.renderTemplate("test_templates/control.txt", "test_output", "control.txt", data);
    
    EXPECT_EQ(readOutputFile("control.txt"), "Hello");

    data["show_greeting"] = false;
    renderer.renderTemplate("test_templates/control.txt", "test_output", "control.txt", data);
    
    EXPECT_EQ(readOutputFile("control.txt"), "Goodbye");
}

TEST_F(TemplateRendererTest, NonexistentTemplate) {
    TemplateRenderer::DataMap data;
    data["name"] = std::string("World");

    EXPECT_THROW(
        renderer.renderTemplate("test_templates/nonexistent.txt", "test_output", "result.txt", data),
        TemplateError
    );
}

TEST_F(TemplateRendererTest, InvalidTemplate) {
    createTemplateFile("invalid.txt", "{{ unclosed_variable");

    TemplateRenderer::DataMap data;
    data["test"] = std::string("value");

    EXPECT_THROW(
        renderer.renderTemplate("test_templates/invalid.txt", "test_output", "result.txt", data),
        TemplateError
    );
}

TEST_F(TemplateRendererTest, EmptyTemplate) {
    createTemplateFile("empty.txt", "");

    TemplateRenderer::DataMap data;
    
    renderer.renderTemplate("test_templates/empty.txt", "test_output", "empty.txt", data);
    
    EXPECT_EQ(readOutputFile("empty.txt"), "");
}

TEST_F(TemplateRendererTest, ArraySupport) {
    createTemplateFile("array.txt",
        "Products:\n"
        "{% for product in products %}"
        "- Name: {{ product.name }}, Price: ${{ product.price }}, "
        "{% if product.in_stock %}In Stock{% else %}Out of Stock{% endif %}\n"
        "{% endfor %}\n"
        "Categories: {% for category in categories %}{{ category }}{% if not loop.is_last %}, {% endif %}{% endfor %}"
    );

    TemplateRenderer::DataMap productData1;
    productData1["name"] = std::string("Laptop");
    productData1["price"] = 999.99;
    productData1["in_stock"] = true;

    TemplateRenderer::DataMap productData2;
    productData2["name"] = std::string("Phone");
    productData2["price"] = 599.99;
    productData2["in_stock"] = false;

    TemplateRenderer::DataArray products;
    products.push_back(productData1);
    products.push_back(productData2);

    TemplateRenderer::DataArray categories;
    categories.push_back(std::string("Electronics"));
    categories.push_back(std::string("Gadgets"));
    categories.push_back(std::string("Tech"));

    TemplateRenderer::DataMap data;
    data["products"] = products;
    data["categories"] = categories;

    renderer.renderTemplate("test_templates/array.txt", "test_output", "array.txt", data);

    std::string expected =
        "Products:\n"
        "- Name: Laptop, Price: $999.99, In Stock"
        "- Name: Phone, Price: $599.99, Out of Stock"
        "Categories: Electronics, Gadgets, Tech";

    EXPECT_EQ(readOutputFile("array.txt"), expected);
}

TEST_F(TemplateRendererTest, NestedArrays) {
    createTemplateFile("nested.txt",
        "{% for department in departments %}"
        "Department: {{ department.name }}\n"
        "Products:\n"
        "{% for product in department.products %}"
        "  - {{ product }}\n"
        "{% endfor %}"
        "{% endfor %}"
    );

    TemplateRenderer::DataArray electronicsProducts;
    electronicsProducts.push_back(std::string("Laptop"));
    electronicsProducts.push_back(std::string("Phone"));

    TemplateRenderer::DataMap electronics;
    electronics["name"] = std::string("Electronics");
    electronics["products"] = electronicsProducts;

    TemplateRenderer::DataArray bookProducts;
    bookProducts.push_back(std::string("Novel"));
    bookProducts.push_back(std::string("Textbook"));

    TemplateRenderer::DataMap books;
    books["name"] = std::string("Books");
    books["products"] = bookProducts;

    TemplateRenderer::DataArray departments;
    departments.push_back(electronics);
    departments.push_back(books);

    TemplateRenderer::DataMap data;
    data["departments"] = departments;

    renderer.renderTemplate("test_templates/nested.txt", "test_output", "nested.txt", data);

    std::string expected =
        "Department: Electronics\n"
        "Products:\n"
        "- Laptop\n"
        "- Phone\n"
        "Department: Books\n"
        "Products:\n"
        "- Novel\n"
        "- Textbook\n";

    EXPECT_EQ(readOutputFile("nested.txt"), expected);
}

} // namespace cppwebforge
