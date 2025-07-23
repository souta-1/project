#define NOMINMAX
#include <windows.h>
#undef min
#undef max



#include "MapEdit.h"
#include <cassert>
#include "Input.h"
#include "DxLib.h"
#include "MapChip.h"
#include <fstream>
#include <codecvt>
#include <sstream>
#include <string>
#include <iostream>
#include <algorithm>



MapEdit::MapEdit()
	:GameObject(), myMap_(MAP_WIDTH* MAP_HEIGHT, -1), //初期値を-1で20*20の配列を初期化する
	isInMapEditArea_(false) //マップエディタ領域内にいるかどうか
{
	mapEditRect_ = { LEFT_MARGIN, TOP_MARGIN,
		MAP_WIDTH * MAP_IMAGE_SIZE, MAP_HEIGHT * MAP_IMAGE_SIZE };
}

MapEdit::~MapEdit()
{
}

void MapEdit::SetMap(Point p, int value)
{
	//マップの座標pにvalueをセットする
	//pが、配列の範囲外の時はassertにひっかかる
	assert(p.x >= 0 && p.x < MAP_WIDTH);
	assert(p.y >= 0 && p.y < MAP_HEIGHT);
	myMap_[p.y * MAP_WIDTH + p.x] = value; //y行x列にvalueをセットする
}

int MapEdit::GetMap(Point p) const
{
	//マップの座標pの値を取得する
	//pが、配列の範囲外の時はassertにひっかかる
	assert(p.x >= 0 && p.x < MAP_WIDTH);
	assert(p.y >= 0 && p.y < MAP_HEIGHT);
	return myMap_[p.y * MAP_WIDTH + p.x]; //y行x列の値を取得する
}

void MapEdit::Update()
{
	Point mousePos;
	if (GetMousePoint(&mousePos.x, &mousePos.y) == -1) {
		return;
	}
	// マウスの座標がマップエディタ領域内にいるかどうかを判定する
	isInMapEditArea_ = mousePos.x >= mapEditRect_.x && mousePos.x <= mapEditRect_.x + mapEditRect_.w &&
		mousePos.y >= mapEditRect_.y && mousePos.y <= mapEditRect_.y + mapEditRect_.h;

	// グリッド座標に変換
	if (!isInMapEditArea_) {
		return; //マップエディタ領域外なら何もしない
	}

	int gridX = (mousePos.x - LEFT_MARGIN) / MAP_IMAGE_SIZE;
	int gridY = (mousePos.y - TOP_MARGIN) / MAP_IMAGE_SIZE;

	drawAreaRect_ = { LEFT_MARGIN + gridX * MAP_IMAGE_SIZE, TOP_MARGIN + gridY * MAP_IMAGE_SIZE,
		MAP_IMAGE_SIZE, MAP_IMAGE_SIZE };

	// --- 範囲選択＋塗りつぶし処理の追加 ---
	static bool isDragging = false;
	static Point dragStartPos = { -1, -1 };
	static Point dragEndPos = { -1, -1 };

	Point mouseGridPos = { gridX, gridY };

	// マップエディタ領域外ならドラッグ状態解除（念のため）
	if (!isInMapEditArea_) {
		isDragging = false;
		return;
	}

	// マウス左ボタン押した瞬間にドラッグ開始位置を記録
	if (Input::IsButtonDown(MOUSE_INPUT_LEFT)) {
		isDragging = true;
		dragStartPos = mouseGridPos;
	}

	// マウス左ボタン押している間はドラッグ中とみなし終了位置を更新
	if (isDragging && Input::IsButtonKeep(MOUSE_INPUT_LEFT)) {
		dragEndPos = mouseGridPos;
	}

	// マウス左ボタン離した瞬間に範囲内を塗りつぶす
	if (isDragging && Input::IsButtonUP(MOUSE_INPUT_LEFT)) {
		isDragging = false;

		int startX = std::min(dragStartPos.x, dragEndPos.x);
		int endX = std::max(dragStartPos.x, dragEndPos.x);
		int startY = std::min(dragStartPos.y, dragEndPos.y);
		int endY = std::max(dragStartPos.y, dragEndPos.y);

		MapChip* mapChip = FindGameObject<MapChip>();
		if (!mapChip || !mapChip->IsHold()) return; // マップチップ持ってないなら何もしない

		int tileToSet = mapChip->GetHoldImage();

		for (int y = startY; y <= endY; y++) {
			for (int x = startX; x <= endX; x++) {
				// 範囲外ガード
				if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) continue;
				SetMap({ x,y }, tileToSet);
			}
		}
		return; // 範囲塗りつぶし時は以降の1セル描画処理をスキップ（重複防止）
	}

	// --- 範囲選択なし時の単セル編集（従来の左クリック塗りつぶし＆シフトで消し） ---
	if (Input::IsButtonKeep(MOUSE_INPUT_LEFT)) //左クリックでマップに値をセット
	{
		MapChip* mapChip = FindGameObject<MapChip>();

		if (CheckHitKey(KEY_INPUT_LSHIFT)) //LShiftキーを押しているなら
		{
			SetMap({ gridX, gridY }, -1); //マップに値をセット（-1は何もない状態）
			return; //マップチップを削除したらここで終了
		}
		else if (mapChip && mapChip->IsHold()) //マップチップを持っているなら
		{
			SetMap({ gridX, gridY }, mapChip->GetHoldImage()); //マップに値をセット
		}
	}

	if (Input::IsKeyDown(KEY_INPUT_S))
	{
		SaveMapData();
	}
	if (Input::IsKeyDown(KEY_INPUT_L))
	{
		LoadMapData();
	}
}

void MapEdit::Draw()
{//背景を描画する

	for (int j = 0; j < MAP_HEIGHT; j++)
	{
		for (int i = 0; i < MAP_WIDTH; i++)
		{
			int value = GetMap({ i,j });
			if (value != -1) //-1なら何も描画しない
			{
				DrawGraph(LEFT_MARGIN + i * MAP_IMAGE_SIZE, TOP_MARGIN + j * MAP_IMAGE_SIZE,
					value, TRUE);
			}
		}
	}

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 128);
	DrawBox(LEFT_MARGIN + 0, TOP_MARGIN + 0,
		LEFT_MARGIN + MAP_WIDTH * MAP_IMAGE_SIZE, TOP_MARGIN + MAP_HEIGHT * MAP_IMAGE_SIZE, GetColor(255, 255, 0), FALSE, 5);
	for (int j = 0; j < MAP_HEIGHT; j++) {
		for (int i = 0; i < MAP_WIDTH; i++) {
			DrawLine(LEFT_MARGIN + i * MAP_IMAGE_SIZE, TOP_MARGIN + j * MAP_IMAGE_SIZE,
				LEFT_MARGIN + (i + 1) * MAP_IMAGE_SIZE, TOP_MARGIN + j * MAP_IMAGE_SIZE, GetColor(255, 255, 255), 1);
			DrawLine(LEFT_MARGIN + i * MAP_IMAGE_SIZE, TOP_MARGIN + j * MAP_IMAGE_SIZE,
				LEFT_MARGIN + i * MAP_IMAGE_SIZE, TOP_MARGIN + (j + 1) * MAP_IMAGE_SIZE, GetColor(255, 255, 255), 1);
		}
	}
	if (isInMapEditArea_) {

		DrawBox(drawAreaRect_.x, drawAreaRect_.y,
			drawAreaRect_.x + drawAreaRect_.w, drawAreaRect_.y + drawAreaRect_.h,
			GetColor(255, 255, 0), TRUE);
	}
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void MapEdit::SaveMapData()
{
	//頑張ってファイル選択ダイアログを出す回
	TCHAR filename[255] = "";
	OPENFILENAME ofn = { 0 };

	ofn.lStructSize = sizeof(ofn);
	//ウィンドウのオーナー＝親ウィンドウのハンドル
	ofn.hwndOwner = GetMainWindowHandle();
	ofn.lpstrFilter = "全てのファイル (*.*)\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = 255;
	ofn.Flags = OFN_OVERWRITEPROMPT;

	if (GetSaveFileName(&ofn))
	{
		printfDx("ファイルが選択された\n");
		//ファイルを開いて、セーブ
		//std::filesystem ファイル名だけ取り出す
		//ofstreamを開く
		std::ofstream openfile(filename);
		//ファイルの選択がキャンセル
		printfDx("セーブがキャンセル\n");
		openfile << "#TinyMapData\n";

		MapChip* mc = FindGameObject<MapChip>();

		for (int j = 0; j < MAP_HEIGHT; j++) {
			for (int i = 0; i < MAP_WIDTH; i++) {

				int index;
				if (myMap_[j * MAP_WIDTH + i] != -1)
					index = mc->GetChipIndex(myMap_[j * MAP_WIDTH + i]);
				else
					index = -1;

				if (i == MAP_WIDTH - 1) //最後の要素なら改行しない
				{
					openfile << index; //最後の要素はカンマをつけない
				}
				else
				{
					//最後の要素以外はカンマをつける
					openfile << index << ",";
				}
			}
			openfile << std::endl;
		}
		openfile.close();
		printfDx("File Saved!!!\n");
	}
}

void MapEdit::LoadMapData()
{
	//頑張ってファイル選択ダイアログを出す回
	TCHAR filename[255] = "";
	OPENFILENAME ifn = { 0 };

	ifn.lStructSize = sizeof(ifn);
	//ウィンドウのオーナー＝親ウィンドウのハンドル
	ifn.hwndOwner = GetMainWindowHandle();
	ifn.lpstrFilter = "全てのファイル (*.*)\0*.*\0";
	ifn.lpstrFile = filename;
	ifn.nMaxFile = 255;
	//ifn.Flags = OFN_OVERWRITEPROMPT;

	//GetOpenFileName()

	if (GetOpenFileName(&ifn))
	{
		printfDx("ファイルが選択された→%s\n", filename);
		//ファイルを開いて、セーブ
		//std::filesystem ファイル名だけ取り出す
		//ifstreamを開く input file stream
		std::ifstream inputfile(filename);
		//ファイルがオープンしたかどうかはチェックが必要
		std::string line;

		//マップチップの情報を取りたい！
		MapChip* mc = FindGameObject<MapChip>();
		myMap_.clear();//マップを空に！
		while (std::getline(inputfile, line)) {
			// 空行はスキップ
			if (line.empty()) continue;
			//printfDx("%s\n", line.c_str());
			//ここに、読み込みの処理を書いていく！
			if (line[0] != '#')
			{
				std::istringstream iss(line);
				std::string tmp;//これに一個ずつ読み込んでいくよ
				while (getline(iss, tmp, ',')) {
					//if(tmp == -1)
					//	myMap_.push_back( -1);
					//else
					//	myMap_.push_back(mc->GetHandle(tmp)); //マップにハンドルをセット
					printfDx("%s ", tmp.c_str());
					if (tmp == "-1")
					{
						myMap_.push_back(-1); //何もない状態
					}
					else
					{
						int index = std::stoi(tmp);
						int handle = mc->GetHandle(index);
						myMap_.push_back(handle); //マップにハンドルをセット
					}
				}
				printfDx("\n");
			}
		}
	}
	else
	{
		//ファイルの選択がキャンセル
		printfDx("セーブがキャンセル\n");
	}
}
