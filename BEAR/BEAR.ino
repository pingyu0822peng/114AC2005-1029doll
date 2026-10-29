// ==============================
// RGB LED 與按鈕設定
// ==============================

const int RED_PIN = 9;
const int GREEN_PIN = 11;
const int BLUE_PIN = 10;
const int BUTTON_PIN = 2;

// 共陽極 RGB LED
const bool COMMON_ANODE = true;

// ==============================
// 顏色設定
// ==============================

// 固定喚醒色：黃色
const float START_R = 75;
const float START_G = 255;
const float START_B = 0;

// ==============================
// 時間設定
// ==============================

const unsigned long idleDelay = 1500;             // 放開後等待 1.5 秒
const unsigned long fadeDuration = 12000;         // 退色共 12 秒
const unsigned long rainbowSpeed = 2500;          // 彩虹變化速度
const unsigned long rainbowDuration = 5000;        // 彩虹模式 5 秒
const unsigned long transitionDuration = 400;      // 顏色平順轉換時間
const unsigned long longPressDuration = 4000;      // 長按 4 秒
const unsigned long debounceDelay = 30;            // 按鈕防抖

const int clicksToRed = 5;                         // 短按 5 次逐漸變紅

// ==============================
// 模式設定
// ==============================

// 0 = 正常亮燈
// 1 = 彩虹模式
// 2 = 自動退色
// 3 = 熄滅
int mode = 3;

// ==============================
// 顏色與時間變數
// ==============================

float currentR = 0;
float currentG = 0;
float currentB = 0;

float transitionStartR = 0;
float transitionStartG = 0;
float transitionStartB = 0;

float transitionTargetR = 0;
float transitionTargetG = 0;
float transitionTargetB = 0;

float fadeStartR = 0;
float fadeStartG = 0;
float fadeStartB = 0;

unsigned long transitionStartTime = 0;
unsigned long fadeStartTime = 0;
unsigned long rainbowStartTime = 0;
unsigned long pressStartTime = 0;
unsigned long releaseTime = 0;

bool isTransitioning = false;
bool isHolding = false;
bool rainbowTriggered = false;
bool wokeFromOff = false;
bool longPressTriggered = false;

int clickCount = 0;

// ==============================
// 按鈕防抖變數
// ==============================

int lastRawButtonState = HIGH;
int stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;

// ==============================
// 初始化
// ==============================

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  setLEDColor(0, 0, 0);
}

// ==============================
// 主程式
// ==============================

void loop() {
  unsigned long now = millis();

  // 更新按鈕狀態
  updateButton(now);

  // 更新顏色平順轉換
  if (isTransitioning) {
    updateColorTransition(now);
  }

  // ==============================
  // 長按 4 秒，啟動彩虹模式
  // ==============================

  if (
    isHolding &&
    !wokeFromOff &&
    !rainbowTriggered &&
    mode == 0 &&
    now - pressStartTime >= longPressDuration
  ) {
    rainbowTriggered = true;
    longPressTriggered = true;

    mode = 1;
    rainbowStartTime = now;
    releaseTime = 0;
    isTransitioning = false;
  }

  // ==============================
  // 彩虹模式持續 5 秒
  // ==============================

  if (mode == 1) {
    if (now - rainbowStartTime >= rainbowDuration) {
      startFade(now);
    } else {
      updateRainbow(now);
    }

    return;
  }

  // ==============================
  // 放開按鈕後等待 1.5 秒
  // ==============================

  if (mode == 0 && releaseTime != 0) {
    unsigned long idleTime = now - releaseTime;

    if (idleTime >= idleDelay) {
      startFade(now);
    }
  }

  // ==============================
  // 自動退色
  // ==============================

  if (mode == 2) {
    float progress =
      (float)(now - fadeStartTime) / fadeDuration;

    if (progress >= 1.0) {
      currentR = 0;
      currentG = 0;
      currentB = 0;

      isTransitioning = false;
      mode = 3;
      releaseTime = 0;

      setLEDColor(0, 0, 0);
      return;
    }

    updateMoodFade(progress);
  }
}

// ==============================
// 按鈕狀態與防抖處理
// ==============================

void updateButton(unsigned long now) {
  int rawState = digitalRead(BUTTON_PIN);

  // 偵測原始按鈕狀態是否改變
  if (rawState != lastRawButtonState) {
    lastDebounceTime = now;
    lastRawButtonState = rawState;
  }

  // 狀態穩定一段時間後才判定有效
  if (now - lastDebounceTime < debounceDelay) {
    return;
  }

  if (rawState == stableButtonState) {
    return;
  }

  stableButtonState = rawState;

  // ==============================
  // 按下按鈕
  // ==============================

  if (stableButtonState == LOW) {
    isHolding = true;
    pressStartTime = now;

    rainbowTriggered = false;
    longPressTriggered = false;

    // 按下時取消先前的自動熄燈倒數
    releaseTime = 0;

    // 如果原本是熄滅狀態，喚醒成黃色
    if (mode == 3) {
      wakeUpLight(now);
    }

    // 如果正在退色，按下時停止退色
    else if (mode == 2) {
      mode = 0;
      isTransitioning = false;

      // 以目前顏色繼續亮燈
      showCurrentColor();
    }
  }

  // ==============================
  // 放開按鈕
  // ==============================

  else {
    isHolding = false;

    // 喚醒後第一次放開，只開始倒數，不轉紅
    if (wokeFromOff) {
      wokeFromOff = false;
      releaseTime = now;
    }

    // 正常短按：逐漸增加紅色
    else if (
      mode == 0 &&
      !longPressTriggered &&
      !rainbowTriggered
    ) {
      clickCount++;

      if (clickCount > clicksToRed) {
        clickCount = clicksToRed;
      }

      float progress =
        (float)clickCount / clicksToRed;

      float targetR =
        START_R + (255.0 - START_R) * progress;

      float targetG =
        START_G * (1.0 - progress);

      float targetB = 0;

      startColorTransition(
        targetR,
        targetG,
        targetB,
        now
      );

      releaseTime = now;
    }

    // 彩虹模式以外，放開後開始倒數
    else if (mode == 0) {
      releaseTime = now;
    }
  }
}

// ==============================
// 喚醒燈光
// ==============================

void wakeUpLight(unsigned long now) {
  mode = 0;

  currentR = START_R;
  currentG = START_G;
  currentB = START_B;

  setLEDColor(START_R, START_G, START_B);

  isTransitioning = false;

  // 記錄這次是從熄滅狀態喚醒
  wokeFromOff = true;

  rainbowTriggered = false;
  longPressTriggered = false;

  clickCount = 0;
  releaseTime = 0;
}

// ==============================
// 開始平順轉換顏色
// ==============================

void startColorTransition(
  float targetR,
  float targetG,
  float targetB,
  unsigned long now
) {
  transitionStartR = currentR;
  transitionStartG = currentG;
  transitionStartB = currentB;

  transitionTargetR = targetR;
  transitionTargetG = targetG;
  transitionTargetB = targetB;

  transitionStartTime = now;
  isTransitioning = true;
}

// ==============================
// 更新平順轉換
// ==============================

void updateColorTransition(unsigned long now) {
  float progress =
    (float)(now - transitionStartTime) /
    transitionDuration;

  if (progress >= 1.0) {
    currentR = transitionTargetR;
    currentG = transitionTargetG;
    currentB = transitionTargetB;

    isTransitioning = false;

    showCurrentColor();
    return;
  }

  currentR =
    transitionStartR +
    (transitionTargetR - transitionStartR) * progress;

  currentG =
    transitionStartG +
    (transitionTargetG - transitionStartG) * progress;

  currentB =
    transitionStartB +
    (transitionTargetB - transitionStartB) * progress;

  showCurrentColor();
}

// ==============================
// 開始自動退色
// ==============================

void startFade(unsigned long now) {
  // 先記錄目前實際顯示的顏色
  if (isTransitioning) {
    updateColorTransition(now);
  }

  fadeStartR = currentR;
  fadeStartG = currentG;
  fadeStartB = currentB;

  fadeStartTime = now;

  isTransitioning = false;
  mode = 2;

  releaseTime = 0;
}

// ==============================
// 退色流程
// 目前顏色 → 黃色 → 綠色 → 藍色 → 熄滅
// ==============================

void updateMoodFade(float progress) {
  progress = constrain(progress, 0.0, 1.0);

  float targetR;
  float targetG;
  float targetB;
  float fraction;

  // 第一階段：目前顏色平順轉為黃色
  if (progress < 0.10) {
    fraction = progress / 0.10;

    targetR =
      fadeStartR +
      (START_R - fadeStartR) * fraction;

    targetG =
      fadeStartG +
      (START_G - fadeStartG) * fraction;

    targetB =
      fadeStartB +
      (START_B - fadeStartB) * fraction;
  }

  // 第二階段：黃色逐漸變綠色
  else if (progress < 0.40) {
    fraction = (progress - 0.10) / 0.30;

    targetR = START_R * (1.0 - fraction);
    targetG = 255;
    targetB = 0;
  }

  // 第三階段：綠色逐漸變藍色
  else if (progress < 0.70) {
    fraction = (progress - 0.40) / 0.30;

    targetR = 0;
    targetG = 255 + (100 - 255) * fraction;
    targetB = 255 * fraction;
  }

  // 第四階段：藍色逐漸熄滅
  else {
    fraction = (progress - 0.70) / 0.30;

    targetR = 0;
    targetG = 100 * (1.0 - fraction);
    targetB = 255 * (1.0 - fraction);
  }

  currentR = targetR;
  currentG = targetG;
  currentB = targetB;

  showCurrentColor();
}

// ==============================
// 彩色霓虹模式
// ==============================

void updateRainbow(unsigned long now) {
  float progress =
    (float)(now - rainbowStartTime) / rainbowSpeed;

  float hue = fmod(progress, 1.0);

  // HSV 色相轉換成 RGB
  float r = 0;
  float g = 0;
  float b = 0;

  float h = hue * 6.0;
  int sector = (int)h;
  float fraction = h - sector;

  switch (sector % 6) {
    case 0:
      r = 255;
      g = 255 * fraction;
      b = 0;
      break;

    case 1:
      r = 255 * (1.0 - fraction);
      g = 255;
      b = 0;
      break;

    case 2:
      r = 0;
      g = 255;
      b = 255 * fraction;
      break;

    case 3:
      r = 0;
      g = 255 * (1.0 - fraction);
      b = 255;
      break;

    case 4:
      r = 255 * fraction;
      g = 0;
      b = 255;
      break;

    case 5:
      r = 255;
      g = 0;
      b = 255 * (1.0 - fraction);
      break;
  }

  currentR = r;
  currentG = g;
  currentB = b;

  showCurrentColor();
}

// ==============================
// 顯示目前顏色
// ==============================

void showCurrentColor() {
  setLEDColor(currentR, currentG, currentB);
}

// ==============================
// RGB LED 輸出
// 共陽極需反轉 PWM 數值
// ==============================

void setLEDColor(float r, float g, float b) {
  int redValue = constrain((int)r, 0, 255);
  int greenValue = constrain((int)g, 0, 255);
  int blueValue = constrain((int)b, 0, 255);

  if (COMMON_ANODE) {
    redValue = 255 - redValue;
    greenValue = 255 - greenValue;
    blueValue = 255 - blueValue;
  }

  analogWrite(RED_PIN, redValue);
  analogWrite(GREEN_PIN, greenValue);
  analogWrite(BLUE_PIN, blueValue);
}