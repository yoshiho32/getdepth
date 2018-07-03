﻿//
// RGB-D カメラからのデプスマップ取得
//

// 標準ライブラリ
#include <Windows.h>

// OpenCV
#include <opencv2/highgui/highgui.hpp>
#ifdef _WIN32
#  define CV_VERSION_STR CVAUX_STR(CV_MAJOR_VERSION) CVAUX_STR(CV_MINOR_VERSION) CVAUX_STR(CV_SUBMINOR_VERSION)
#  ifdef _DEBUG
#    define CV_EXT_STR "d.lib"
#  else
#    define CV_EXT_STR ".lib"
#  endif
#  pragma comment(lib, "opencv_world" CV_VERSION_STR CV_EXT_STR)
#endif

// ウィンドウ関連の処理
#include "GgApplication.h"

// 描画に用いるメッシュ
#include "Mesh.h"

// 計算に用いるシェーダ
#include "Calculate.h"
#include "Compute.h"

// センサ関連の処理
#include "KinectV2.h"

// OpenCV によるビデオキャプチャに使うカメラ
#define CAPTURE_DEVICE 0

// 頂点位置の生成をシェーダ (position.frag) で行うなら 1
#define GENERATE_POSITION 1

// バイラテラルフィルタを使うなら 1
#define USE_FILTER 0

// 透明人間にするなら 1
#define USE_REFRACTION 0

// カメラパラメータ
constexpr GLfloat cameraFovy(1.0f);                     // 画角
constexpr GLfloat cameraNear(0.1f);                     // 前方面までの距離
constexpr GLfloat cameraFar(50.0f);                     // 後方面までの距離

// 光源
constexpr GgSimpleShader::Light lightData =
{
  { 0.2f, 0.2f, 0.2f, 1.0f },                           // 環境光成分
  { 1.0f, 1.0f, 1.0f, 1.0f },                           // 拡散反射光成分
  { 1.0f, 1.0f, 1.0f, 1.0f },                           // 鏡面光成分
  { 0.0f, 0.0f, 5.0f, 1.0f }                            // 位置
};

// 材質
constexpr GgSimpleShader::Material materialData =
{
  { 0.8f, 0.8f, 0.8f, 1.0f },                           // 環境光の反射係数
  { 0.8f, 0.8f, 0.8f, 1.0f },                           // 拡散反射係数
  { 0.2f, 0.2f, 0.2f, 1.0f },                           // 鏡面反射係数
  50.0f                                                 // 輝き係数
};

// 背景色
constexpr GLfloat background[] = { 0.2f, 0.3f, 0.4f, 0.0f };

// バイラテラルフィルタのデフォルトの分散
constexpr GLfloat variance(0.1f);

//
// アプリケーションの実行
//
void GgApplication::run()
{
  // ウィンドウを開く
  Window window("Depth Map Viewer");
  if (!window.get()) throw std::runtime_error("GLFW のウィンドウが開けません");

  // 深度センサを有効にする
  SENSOR sensor;
  if (sensor.getActivated() == 0) throw std::runtime_error("深度センサを有効にできません");

  // 深度センサの解像度
  int width, height;
  sensor.getDepthResolution(&width, &height);

  // 描画に使うメッシュ
  const Mesh mesh(width, height, sensor.getCoordBuffer());

  // 描画用のシェーダ
#if USE_REFRACTION
  const GgSimpleShader simple("refraction.vert", "refraction.frag");
  const GLint sizeLoc(glGetUniformLocation(simple.get(), "size"));

  // 背景画像のテクスチャ
  GLuint bmap;
  glGenTextures(1, &bmap);
  glBindTexture(GL_TEXTURE_2D, bmap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, capture_env_width, capture_env_height, 0, GL_BGR, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#else
  const GgSimpleShader simple("simple.vert", "simple.frag");
#endif

  // 光源データ
  const GgSimpleShader::LightBuffer light(lightData);

  // 材質データ
  const GgSimpleShader::MaterialBuffer material(materialData);

  // 頂点位置から法線ベクトルを計算するシェーダ
  const Calculate normal(width, height, "normal.frag");

//#if USE_FILTER
  // フィルターのシェーダ
  //const Compute bilateral(width, height, "bilateral.comp");
//#endif

  // 背景色を設定する
  glClearColor(background[0], background[1], background[2], background[3]);

  // 隠面消去処理を有効にする
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  // ウィンドウが開いている間くり返し描画する
  while (!window.shouldClose())
  {
#if USE_REFRACTION
    // 画像のキャプチャ
    if (camera.grab())
    {
      // キャプチャ映像から画像を切り出す
      cv::Mat frame;
      camera.retrieve(frame, 3);

      // 切り出した画像をテクスチャに転送する
      cv::Mat flipped;
      cv::flip(frame, flipped, 0);
      glBindTexture(GL_TEXTURE_2D, bmap);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.cols, flipped.rows, GL_BGR, GL_UNSIGNED_BYTE, flipped.data);
    }
#endif

    // 頂点位置の計算
    sensor.setVariance(variance + static_cast<GLfloat>(window.getArrowY()) * 0.01f);
    const GLuint positionTexture(sensor.getPosition());

#if USE_FILTER
	//バイラテラルフィルターの計算
	bilateral.use();
	//glUniform1i(0, 0);
	//glActiveTexture(GL_TEXTURE0);
	//glBindTexture(GL_TEXTURE_2D, positionTexture);
	const GLuint filteredTexture(bilateral.execute(positionTexture,GL_RGBA32F)[0]);
#endif

    // 法線ベクトルの計算
    normal.use();
    glUniform1i(0, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture);
    const GLuint normalTexture(normal.execute()[0]);

    // 画面消去
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ビューポートをもとに戻す
    window.resetViewport();

    // プロジェクション変換行列
    const GgMatrix mp(ggPerspective(cameraFovy, window.getAspect(), cameraNear, cameraFar));

    // モデルビュー変換行列
    const GgMatrix mw(window.getTrackball(1) * window.getTranslation(0));

    // 描画用のシェーダプログラムの使用開始
    simple.use();
    simple.loadMatrix(mp, mw);
    simple.selectLight(light);
    simple.selectMaterial(material);

    // 頂点座標テクスチャ
    glUniform1i(0, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture);

    // 法線ベクトルテクスチャ
    glUniform1i(1, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture);

#if USE_REFRACTION
    // ウィンドウサイズ
    glUniform2f(sizeLoc, static_cast<GLfloat>(window.getWidth()), static_cast<GLfloat>(window.getHeight()));

    // 背景テクスチャ
    glUniform1i(3, 3);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, bmap);
#else
    // 前景テクスチャ
    glUniform1i(2, 2);
    glActiveTexture(GL_TEXTURE2);
    sensor.getColor();

    // 疑似カラー処理
    glUniform2fv(3, 1, sensor.range);
#endif

    // 図形描画
    mesh.draw();

    // バッファを入れ替える
    window.swapBuffers();
  }
}
