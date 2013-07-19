/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsColorPicker.h"

#include <shlwapi.h>

#include "nsIWidget.h"
#include "WidgetUtils.h"

using namespace mozilla::widget;

namespace
{
// Manages NS_NATIVE_TMP_WINDOW child windows. NS_NATIVE_TMP_WINDOWs are
// temporary child windows of mParentWidget created to address RTL issues
// in picker dialogs. We are responsible for destroying these.
class AutoDestroyTmpWindow
{
public:
  explicit AutoDestroyTmpWindow(HWND aTmpWnd) :
    mWnd(aTmpWnd) {
  }

  ~AutoDestroyTmpWindow() {
    if (mWnd)
      DestroyWindow(mWnd);
  }

  inline HWND get() const { return mWnd; }
private:
  HWND mWnd;
};

static DWORD ColorStringToRGB(const nsAString& aColor)
{
  DWORD result = 0;

  for (uint32_t i = 1; i < aColor.Length(); ++i) {
    result *= 16;

    PRUnichar c = aColor[i];
    if (c >= '0' && c <= '9') {
      result += c - '0';
    } else if (c >= 'a' && c <= 'f') {
      result += 10 + (c - 'a');
    } else {
      result += 10 + (c - 'A');
    }
  }

  DWORD r = result & 0x00FF0000;
  DWORD g = result & 0x0000FF00;
  DWORD b = result & 0x000000FF;

  r = r >> 16;
  b = b << 16;

  result = r | g | b;

  return result;
}

static nsString ToHexString(BYTE n)
{
  nsString result;
  if (n <= 0x0F) {
    result.Append('0');
  }
  result.AppendInt(n, 16);
  return result;
}


static void
BGRIntToRGBString(DWORD color, nsAString& aResult)
{
  BYTE r = GetRValue(color);
  BYTE g = GetGValue(color);
  BYTE b = GetBValue(color);

  aResult.AssignLiteral("#");
  aResult.Append(ToHexString(r));
  aResult.Append(ToHexString(g));
  aResult.Append(ToHexString(b));
}
} // anonymous namespace

AsyncColorChooser::AsyncColorChooser(DWORD aInitialColor, nsIWidget* aParentWidget, nsIColorPickerShownCallback* aCallback)
  : mInitialColor(aInitialColor)
  , mParentWidget(aParentWidget)
  , mCallback(aCallback)
{
}

NS_IMETHODIMP
AsyncColorChooser::Run()
{
  CHOOSECOLOR options;
  static COLORREF customColors[16] = {0} ;

  AutoDestroyTmpWindow adtw((HWND) (mParentWidget.get() ?
    mParentWidget->GetNativeData(NS_NATIVE_TMP_WINDOW) : NULL));

  options.lStructSize   = sizeof(options);
  options.hwndOwner     = adtw.get();
  options.Flags         = CC_RGBINIT | CC_FULLOPEN;
  options.rgbResult     = mInitialColor;
  options.lpCustColors  = customColors;

  if (ChooseColor(&options)) {
    BGRIntToRGBString(options.rgbResult, mColor);
  }

  if (mCallback) {
    mCallback->Done(mColor);
  }

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIColorPicker

nsColorPicker::nsColorPicker()
{
}

nsColorPicker::~nsColorPicker()
{
}

NS_IMPL_ISUPPORTS1(nsColorPicker, nsIColorPicker)

NS_IMETHODIMP
nsColorPicker::Init(nsIDOMWindow* parent, const nsAString& title, const nsAString& aInitialColor)
{
  NS_PRECONDITION(parent,
      "Null parent passed to colorpicker, no color picker for you!");
  mParentWidget =  WidgetUtils::DOMWindowToWidget(parent);
  mInitialColor = ColorStringToRGB(aInitialColor);
  return NS_OK;
}

NS_IMETHODIMP
nsColorPicker::Open(nsIColorPickerShownCallback* aCallback)
{
  NS_ENSURE_ARG(aCallback);
  nsCOMPtr<nsIRunnable> event = new AsyncColorChooser(mInitialColor, mParentWidget, aCallback);
  return NS_DispatchToMainThread(event);
}
