﻿//
// Kinect (v2) のデプスマップ取得
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

// センサ関連の処理
//#include "KinectV1.h"
//#include "KinectV2.h"
#include "Ds325.h"

// 各種設定
#include "config.h"

// ウィンドウ関連の処理
#include "GgApplication.h"

// 描画に用いるメッシュ
#include "Mesh.h"

// 計算に用いるシェーダ
#include "Calculate.h"
#include "Compute.h"

// 頂点位置の生成をシェーダ (position.frag) で行うなら 1
#define GENERATE_POSITION 1

//
// アプリケーションの実行
//
void GgApplication::run()
{
  // OpenCV によるビデオキャプチャを初期化する
  cv::VideoCapture camera(0);
  if (!camera.isOpened()) throw "ビデオカメラが見つかりません。";

  // カメラの初期設定
  camera.grab();
  const GLsizei capture_env_width(GLsizei(camera.get(CV_CAP_PROP_FRAME_WIDTH)));
  const GLsizei capture_env_height(GLsizei(camera.get(CV_CAP_PROP_FRAME_HEIGHT)));

  // ウィンドウを開く
  Window window("Depth Map Viewer");
  if (!window.get()) throw "GLFW のウィンドウが開けませんでした。";

  // 深度センサを有効にする
#if USE_KINECT_V1
#  define POSITION_SHADER "position_v1"
  KinectV1 sensor;
#elif USE_KINECT_V2
#  define POSITION_SHADER "position_v2"
  KinectV2 sensor;
#elif USE_DEPTH_SENSE
#  define POSITION_SHADER "position_ds"
  Ds325 sensor;
#endif
  if (sensor.getActivated() == 0) throw "深度センサを有効にできませんでした。";

  // 深度センサの解像度
  int width, height;
  sensor.getDepthResolution(&width, &height);

  // 描画に使うメッシュ
  const Mesh mesh(width, height, sensor.getCoordBuffer());

  // 描画用のシェーダ
  const GgSimpleShader simple("simple.vert", "simple.frag");
  //const GgSimpleShader simple("refraction.vert", "refraction.frag");
  //const GLint sizeLoc(glGetUniformLocation(simple.get(), "size"));

  // 光源データ
  const GgSimpleShader::LightBuffer light(lightData);

  // 材質データ
  const GgSimpleShader::MaterialBuffer material(materialData);

  // デプスデータから頂点位置を計算するシェーダ
  const Calculate position(width, height, POSITION_SHADER ".frag");
  const GLuint scaleLoc(glGetUniformLocation(position.get(), "scale"));
  const GLuint depthMaxLoc(glGetUniformLocation(position.get(), "depthMax"));
  const GLuint depthScaleLoc(glGetUniformLocation(position.get(), "depthScale"));

  // バイラテラルフィルタのシェーダ
  //const Calculate bilateral(width, height, "bilateral.frag");
  const Compute bilateral(width, height, "bilateral.comp");
  const GLint varianceLoc(glGetUniformLocation(bilateral.get(), "variance"));

  // 頂点位置から法線ベクトルを計算するシェーダ
  const Calculate normal(width, height, "normal.frag");

  // 背景画像のテクスチャ
  GLuint bmap;
  glGenTextures(1, &bmap);
  glBindTexture(GL_TEXTURE_2D, bmap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, capture_env_width, capture_env_height, 0, GL_BGR, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  // 背景色を設定する
  glClearColor(background[0], background[1], background[2], background[3]);

  // 隠面消去処理を有効にする
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  // ウィンドウが開いている間くり返し描画する
  while (!window.shouldClose())
  {
#if GENERATE_POSITION
    // 頂点位置の計算
    position.use();
    glUniform4fv(scaleLoc, 1, sensor.getScale());
    glUniform1i(0, 0);
    glActiveTexture(GL_TEXTURE0);
    sensor.getDepth();
    const std::vector<GLuint> &positionTexture(position.execute());

    // バイラテラルフィルタ
    bilateral.use();
    glUniform1f(varianceLoc, static_cast<GLfloat>(window.getArrowY()));
    glUniform1i(0, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture[0]);
    const std::vector<GLuint> &bilateralTexture(bilateral.execute(positionTexture[0], GL_RGBA32F));

    // 法線ベクトルの計算
    normal.use();
    glUniform1i(0, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture[0]);
    const std::vector<GLuint> &normalTexture(normal.execute());
#else
    // バイラテラルフィルタ
    bilateral.use();
    glUniform1f(varianceLoc, static_cast<GLfloat>(window.getArrowY()));
    glUniform1i(0, 0);
    glActiveTexture(GL_TEXTURE0);
    sensor.getPoint();
    const std::vector<GLuint> &bilateralTexture(bilateral.execute());

    // 法線ベクトルの計算
    normal.use();
    glUniform1i(0, 0);
    glActiveTexture(GL_TEXTURE0);
    sensor.getPoint();
    const std::vector<GLuint> &normalTexture(normal.execute());
#endif

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

#if GENERATE_POSITION
    // 頂点座標テクスチャ
    glUniform1i(0, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positionTexture[0]);
#endif

    // 法線ベクトルテクスチャ
    glUniform1i(1, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture[0]);

    // 背景テクスチャ
    glUniform1i(2, 2);
    glActiveTexture(GL_TEXTURE2);
    sensor.getColor();

    // 図形描画
    mesh.draw();

    // バッファを入れ替える
    window.swapBuffers();
  }
}