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
