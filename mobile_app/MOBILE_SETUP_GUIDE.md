# Task 4: Mobile App Installation & Setup Guide (Flutter on Windows)

Step-by-step click-by-click instructions for building and running the Flutter app on Android and iOS.

---

## 1. Installing Flutter on Windows

1. **Download Flutter SDK:**
   - Go to [docs.flutter.dev/get-started/install/windows](https://docs.flutter.dev/get-started/install/windows/mobile)
   - Download the Flutter SDK zip bundle.
   - Extract it to `C:\src\flutter`.

2. **Add Flutter to Windows Path:**
   - Open Windows Search -> type **Environment Variables** (تعديل متغيرات البيئة).
   - Edit `Path` under User variables -> Add `C:\src\flutter\bin`.

3. **Install Android Studio:**
   - Download Android Studio from [developer.android.com/studio](https://developer.android.com/studio).
   - Install standard components & Android SDK.
   - Open Android Studio -> Tools -> SDK Manager -> Install **Android SDK Command-line Tools**.

---

## 2. Setting Up Firebase in Flutter

1. **Add Android App in Firebase Console:**
   - Go to your Firebase project -> Click **Add App** -> Select **Android** (🤖 icon).
   - Android package name: `com.example.bond_touch_app`.
   - Download `google-services.json` and place it inside `mobile_app/android/app/google-services.json`.

---

## 3. Running the App on Your Android Phone

1. **Enable Developer Options on Android:**
   - On your phone: Settings -> About Phone -> Tap **Build Number** 7 times until Developer Mode turns on.
   - Enable **USB Debugging** (تصحيح الأخطاء عبر USB).

2. **Connect Phone & Run Command:**
   - Connect your phone to your PC via USB cable.
   - Open terminal in `mobile_app` folder and run:
   ```bash
   flutter pub get
   flutter run
   ```
   - Select your connected phone device. The app will build and install automatically!
