// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/renderer/autofill/password_generation_manager.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebInputElement;
using WebKit::WebNode;
using WebKit::WebString;

namespace autofill {

class TestPasswordGenerationManager : public PasswordGenerationManager {
 public:
  explicit TestPasswordGenerationManager(content::RenderView* view)
      : PasswordGenerationManager(view) {}
  virtual ~TestPasswordGenerationManager() {}

  // Make this public
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return PasswordGenerationManager::OnMessageReceived(message);
  }

  const std::vector<IPC::Message*>& messages() {
    return messages_.get();
  }

 protected:
  virtual bool ShouldAnalyzeDocument(const WebKit::WebDocument& document) const
      OVERRIDE {
    return true;
  }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    messages_.push_back(message);
    return true;
  }

 private:
  ScopedVector<IPC::Message> messages_;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordGenerationManager);
};

class PasswordGenerationManagerTest : public ChromeRenderViewTest {
 public:
  PasswordGenerationManagerTest() {}

  virtual void SetUp() {
    // We don't actually create a PasswordGenerationManager during
    // ChromeRenderViewTest::SetUp because it's behind a flag. Since we want
    // to use a test manager anyway, we just create out own.
    ChromeRenderViewTest::SetUp();
    generation_manager_.reset(new TestPasswordGenerationManager(view_));
  }

  void SimulateClickOnDecoration(WebKit::WebInputElement* input_element) {
    WebKit::WebElement decoration =
        input_element->decorationElementFor(generation_manager_.get());
    decoration.simulateClick();
  }

  bool DecorationIsVisible(WebKit::WebInputElement* input_element) {
    WebKit::WebElement decoration =
        input_element->decorationElementFor(generation_manager_.get());
    return decoration.hasNonEmptyBoundingBox();
  }

 protected:
  scoped_ptr<TestPasswordGenerationManager> generation_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationManagerTest);
};

const char kSigninFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'password'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password'/> "
    "  <INPUT type = 'password' id = 'second_password'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

TEST_F(PasswordGenerationManagerTest, DetectionTest) {
  LoadHTML(kSigninFormHTML);

  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement password_element = element.to<WebInputElement>();
  EXPECT_FALSE(DecorationIsVisible(&password_element));

  LoadHTML(kAccountCreationFormHTML);

  // We don't show the decoration yet because the feature isn't enabled.
  document = GetMainFrame()->document();
  element = document.getElementById(WebString::fromUTF8("first_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement first_password_element = element.to<WebInputElement>();
  element = document.getElementById(WebString::fromUTF8("second_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement second_password_element = element.to<WebInputElement>();
  EXPECT_FALSE(DecorationIsVisible(&first_password_element));

  // Pretend like password generation was enabled.
  AutofillMsg_PasswordGenerationEnabled msg(0, true);
  generation_manager_->OnMessageReceived(msg);

  // Now check that the decoration is visible on the first password field and
  // that we send a message after the decoration is clicked.
  LoadHTML(kAccountCreationFormHTML);
  document = GetMainFrame()->document();
  element = document.getElementById(WebString::fromUTF8("first_password"));
  ASSERT_FALSE(element.isNull());
  first_password_element = element.to<WebInputElement>();
  element = document.getElementById(WebString::fromUTF8("second_password"));
  ASSERT_FALSE(element.isNull());
  second_password_element = element.to<WebInputElement>();
  EXPECT_TRUE(DecorationIsVisible(&first_password_element));
  SimulateClickOnDecoration(&first_password_element);
  EXPECT_EQ(1u, generation_manager_->messages().size());
  EXPECT_EQ(AutofillHostMsg_ShowPasswordGenerationPopup::ID,
            generation_manager_->messages()[0]->type());
}

TEST_F(PasswordGenerationManagerTest, FillTest) {
  // Make sure that we are enabled before loading HTML.
  AutofillMsg_PasswordGenerationEnabled enabled_msg(0, true);
  generation_manager_->OnMessageReceived(enabled_msg);
  LoadHTML(kAccountCreationFormHTML);

  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("first_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement first_password_element = element.to<WebInputElement>();
  element = document.getElementById(WebString::fromUTF8("second_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement second_password_element = element.to<WebInputElement>();

  // Both password fields should be empty.
  EXPECT_TRUE(first_password_element.value().isNull());
  EXPECT_TRUE(second_password_element.value().isNull());

  string16 password = ASCIIToUTF16("random_password");
  AutofillMsg_GeneratedPasswordAccepted msg(0, password);
  generation_manager_->OnMessageReceived(msg);

  // Password fields are filled out and set as being autofilled.
  EXPECT_EQ(password, first_password_element.value());
  EXPECT_EQ(password, second_password_element.value());
  EXPECT_TRUE(first_password_element.isAutofilled());
  EXPECT_TRUE(second_password_element.isAutofilled());
}

}  // namespace autofill
