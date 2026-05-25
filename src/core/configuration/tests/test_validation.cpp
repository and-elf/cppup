#include <gtest/gtest.h>

#include "../validation.hpp"

using namespace cppup::configuration;

TEST(ValidationResult, DefaultIsValidWithNoErrorsOrWarnings)
{
  ValidationResult const result;
  EXPECT_TRUE(result.is_valid);
  EXPECT_FALSE(result.has_errors());
  EXPECT_FALSE(result.has_warnings());
  EXPECT_EQ(result.error_count(), 0U);
  EXPECT_EQ(result.warning_count(), 0U);
}

TEST(ValidationResult, AddErrorMarksInvalid)
{
  ValidationResult result;
  result.add_error(ValidationErrorType::PackageNotFound, "Package not found", "Install package");

  EXPECT_FALSE(result.is_valid);
  EXPECT_TRUE(result.has_errors());
  ASSERT_EQ(result.error_count(), 1U);
  EXPECT_EQ(result.errors[0].type, ValidationErrorType::PackageNotFound);
  EXPECT_EQ(result.errors[0].message, "Package not found");
  EXPECT_EQ(result.errors[0].suggestion, "Install package");
}

TEST(ValidationResult, AddWarningKeepsValidity)
{
  ValidationResult result;
  result.add_warning(ValidationErrorType::Warning, "Warning message", "Fix warning");

  EXPECT_TRUE(result.is_valid);
  EXPECT_TRUE(result.has_warnings());
  ASSERT_EQ(result.warning_count(), 1U);
  EXPECT_EQ(result.warnings[0].type, ValidationErrorType::Warning);
  EXPECT_EQ(result.warnings[0].message, "Warning message");
}

TEST(ConfigurationValidator, EmptyConfigurationIsValid)
{
  ConfigurationValidator const validator;
  BuildConfiguration const     config;
  auto                         result = validator.validate(config);
  EXPECT_TRUE(result.is_valid);
}

TEST(ConfigurationValidator, EmptyBuildStepNameIsInvalid)
{
  ConfigurationValidator const validator;
  BuildConfiguration           config;
  config.build_steps.emplace_back("", []() {});

  auto result = validator.validate(config);
  EXPECT_FALSE(result.is_valid);
  EXPECT_TRUE(result.has_errors());
}

TEST(ConfigurationValidator, TestWithEmptyFrameworkIsValid)
{
  ConfigurationValidator const validator;
  BuildConfiguration           config;
  config.tests.push_back(cppup::configuration::Test{.name = "plain", .sources = {"plain.cpp"}});

  auto result = validator.validate(config);
  EXPECT_TRUE(result.is_valid) << "Tests with no framework should pass validation";
}

TEST(ConfigurationValidator, TestReferencingDeclaredFrameworkIsValid)
{
  ConfigurationValidator const validator;
  BuildConfiguration           config;
  config.test_frameworks.push_back(TestFramework{.name = "gtest", .plugin = "gtest"});
  config.tests.push_back(cppup::configuration::Test{
      .name = "uses_gtest", .sources = {"uses_gtest.cpp"}, .framework = "gtest"});

  auto result = validator.validate(config);
  EXPECT_TRUE(result.is_valid);
}

TEST(ConfigurationValidator, TestReferencingUndeclaredFrameworkIsInvalid)
{
  ConfigurationValidator const validator;
  BuildConfiguration           config;
  config.tests.push_back(cppup::configuration::Test{
      .name = "uses_missing", .sources = {"x.cpp"}, .framework = "missing"});

  auto result = validator.validate(config);
  EXPECT_FALSE(result.is_valid);
  ASSERT_TRUE(result.has_errors());
  EXPECT_EQ(result.errors[0].type, ValidationErrorType::TestFrameworkNotFound);
  EXPECT_NE(result.errors[0].message.find("uses_missing"), std::string::npos);
  EXPECT_NE(result.errors[0].message.find("missing"), std::string::npos);
}

TEST(ConfigurationValidator, TestFrameworkWithEmptyNameIsInvalid)
{
  ConfigurationValidator const validator;
  BuildConfiguration           config;
  config.test_frameworks.push_back(TestFramework{.name = "", .plugin = "gtest"});

  auto result = validator.validate(config);
  EXPECT_FALSE(result.is_valid);
  EXPECT_TRUE(result.has_errors());
}

TEST(ConfigurationValidator, DuplicateTestFrameworkNamesAreInvalid)
{
  ConfigurationValidator const validator;
  BuildConfiguration           config;
  config.test_frameworks.push_back(TestFramework{.name = "gtest", .plugin = "gtest"});
  config.test_frameworks.push_back(TestFramework{.name = "gtest", .plugin = "gtest"});

  auto result = validator.validate(config);
  EXPECT_FALSE(result.is_valid);
  EXPECT_TRUE(result.has_errors());
}
