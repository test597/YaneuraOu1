﻿#ifndef __LEARN_WEIGHT_H__
#define __LEARN_WEIGHT_H__

// 評価関数の機械学習のときに用いる重み配列などに関する機械学習用ツール類一式

#include "learn.h"

namespace EvalLearningTools
{
	// -------------------------------------------------
	//                     初期化
	// -------------------------------------------------

	// このEvalLearningTools名前空間にあるテーブル類を初期化する。
	// 学習の開始までに必ず一度呼び出すこと。
	void init();

	// -------------------------------------------------
	//       勾配等を格納している学習用の配列
	// -------------------------------------------------

#if defined(_MSC_VER)
#pragma pack(push,2)
#elif defined(__GNUC__)
#pragma pack(2)
#endif
	struct Weight
	{
		// mini-batch 1回分の勾配の累積値
		std::array<LearnFloatType, 2> g;

		// ADA_GRAD_UPDATEのとき。LearnFloatType == floatとして、
		// 合計 4*2 + 4*2 + 1*2 = 18 bytes
		// 1GBの評価関数パラメーターに対してその4.5倍のサイズのWeight配列が確保できれば良い。
		// ただし、構造体のアライメントが4バイト単位になっているとsizeof(Weight)==20なコードが生成されるので
		// pragma pack(2)を指定しておく。

		// SGD_UPDATE の場合、この構造体はさらに10バイト減って、8バイトで済む。

#if defined (ADA_GRAD_UPDATE)

		// AdaGradの学習率η(eta)。
		// updateFV()が呼び出されるまでに設定されているものとする。
		static double eta;

		// AdaGradのg2
		std::array<LearnFloatType, 2> g2;

		// vの小数部上位8bit。(vをfloatで持つのもったいないのでvの補助bitとして8bitで持つ)
		std::array<s8, 2> v8;

		// AdaGradでupdateする
		// この関数を実行しているときにgの値やメンバーが書き変わらないことは
		// 呼び出し側で保証されている。atomic演算である必要はない。
		template <typename T>
		void updateFV(std::array<T, 2>& v)
		{
			// AdaGradの更新式
			//   勾配ベクトルをg、更新したいベクトルをv、η(eta)は定数として、
			//     g2 = g2 + g^2
			//     v = v - ηg/sqrt(g2)

			constexpr double epsilon = 0.000001;
			for (int i = 0; i < 2; ++i)
			{
				if (g[i] == LearnFloatType(0))
					continue;

				g2[i] += g[i] * g[i];

				// v8は小数部8bitを含んでいるのでこれを復元する。
				// 128倍にすると、-1を保持できなくなるので127倍にしておく。
				// -1.0～+1.0を-127～127で保持している。
				// std::round()限定なら-0.5～+0.5の範囲なので255倍でも良いが、
				// どんな丸め方をするかはわからないので余裕を持たせてある。

				double V = v[i] + ((double)v8[i] / 127);

				V -= eta * (double)g[i] / sqrt((double)g2[i] + epsilon);

				// Vの値をINT16の範囲に収まるように制約を課す。
				V = min((double)INT16_MAX * 3 / 4, V);
				V = max((double)INT16_MIN * 3 / 4, V);

				v[i] = (T)round(V);
				v8[i] = (s8)((V - v[i]) * 127);

				// この要素に関するmini-batchの1回分の更新が終わったのでgをクリア
				//g[i] = 0;
				// これは呼び出し側で行なうことにする。
			}
		}

#elif defined(SGD_UPDATE)

		static PRNG rand;

		// 勾配の符号だけ見るSGDでupdateする
		// この関数を実行しているときにgの値やメンバーが書き変わらないことは
		// 呼び出し側で保証されている。atomic演算である必要はない。
		template <typename T>
		void updateFV(array<T, 2>& v)
		{
			for (int i = 0; i < 2; ++i)
			{
				if (g[i] == 0)
					continue;

				// g[i]の符号だけ見てupdateする。
				// g[i] < 0 なら vを少し足す。
				// g[i] > 0 なら vを少し引く。

				// 整数しか足さないので小数部不要。

				// しかし+1,-1だと値が動きすぎるので 1/3ぐらいの確率で動かす。
				if (rand.rand(3))
					continue;

				auto V = v[i];
				if (g[i] > 0.0)
					V--;
				else
					V++;

				// Vの値をINT16の範囲に収まるように制約を課す。
				V = min((s16)((double)INT16_MAX * 3 / 4), (s16)(V));
				V = max((s16)((double)INT16_MIN * 3 / 4), (s16)(V));

				v[i] = (T)V;
			}
		}

#endif
	};
#if defined(_MSC_VER)
#pragma pack(pop)
#elif defined(__GNUC__)
#pragma pack(0)
#endif

	// -------------------------------------------------
	//                  tables
	// -------------------------------------------------

	// 	--- BonaPieceに対してMirrorとInverseを提供する配列。

	// これらの配列は、init();を呼び出すと初期化される。
	// これらの配列は、以下のKK/KKP/KPPクラスから参照される。

	// あるBonaPieceを相手側から見たときの値
	extern Eval::BonaPiece inv_piece[Eval::fe_end];

	// 盤面上のあるBonaPieceをミラーした位置にあるものを返す。
	extern Eval::BonaPiece mir_piece[Eval::fe_end];

	// 次元下げしたときに、そのなかの一番小さなindexになることが
	// わかっているindexに対してtrueとなっているフラグ配列。
	// この配列もinit()によって初期化される。
	extern std::vector<bool> min_index_flag;

	// -------------------------------------------------
	// Weight配列を直列化したときのindexを計算したりするヘルパー。
	// -------------------------------------------------

	// 注意 : 上記のinv_piece/mir_pieceを間接的に参照するので、
	// 最初にinit()を呼び出して初期化すること。

	struct KK
	{
		KK() {}
		KK(Square king0, Square king1) : king0_(king0), king1_(king1) {}

		// KK,KKP,KPP配列を直列化するときの通し番号の、KKの最小値、最大値。
		static u64 min_index() { return 0; }
		static u64 max_index() { return min_index() + (u64)SQ_NB*(u64)SQ_NB; }

		// 与えられたindexが、min_index()以上、max_index()未満にあるかを判定する。
		static bool is_ok(u64 index) { return min_index() <= index && index < max_index(); }

		// indexからKKのオブジェクトを生成するbuilder
		static KK fromIndex(u64 index)
		{
			index -= min_index();
			Square king1 = (Square)(index % SQ_NB);
			index /= SQ_NB;
			Square king0 = (Square)(index  /* % SQ_NB */);
			ASSERT_LV3(king0 < SQ_NB);
			return KK(king0, king1);
		}

		// fromIndex()を用いてこのオブジェクトを構築したときに、以下のアクセッサで情報が得られる。
		Square king0() const { return king0_; }
		Square king1() const { return king1_; }

		// 低次元の配列のindexを得る。
		// KKはミラーの次元下げを行わないので、そのままの値。
		void toLowerDimensions(/*out*/KK kk_[1]) const {
			kk_[0] = KK(king0_, king1_);
		}

		// 現在のメンバの値に基いて、直列化されたときのindexを取得する。
		u64 toIndex() const {
			return min_index() + (u64)king0_ * (u64)SQ_NB + (u64)king1_;
		}

	private:
		Square king0_, king1_;
	};

	struct KKP
	{
		KKP() {}
		KKP(Square king0, Square king1, Eval::BonaPiece p) : king0_(king0), king1_(king1), piece_(p) {}

		// KK,KKP,KPP配列を直列化するときの通し番号の、KKPの最小値、最大値。
		static u64 min_index() { return KK::max_index(); }
		static u64 max_index() { return min_index() + (u64)SQ_NB*(u64)SQ_NB*(u64)Eval::fe_end; }

		// 与えられたindexが、min_index()以上、max_index()未満にあるかを判定する。
		static bool is_ok(u64 index) { return min_index() <= index && index < max_index(); }

		// indexからKKPのオブジェクトを生成するbuilder
		static KKP fromIndex(u64 index)
		{
			index -= min_index();
			Eval::BonaPiece piece = (Eval::BonaPiece)(index % Eval::fe_end);
			index /= Eval::fe_end;
			Square king1 = (Square)(index % SQ_NB);
			index /= SQ_NB;
			Square king0 = (Square)(index  /* % SQ_NB */);
			ASSERT_LV3(king0 < SQ_NB);
			return KKP(king0, king1, piece);
		}

		// fromIndex()を用いてこのオブジェクトを構築したときに、以下のアクセッサで情報が得られる。
		Square king0() const { return king0_; }
		Square king1() const { return king1_; }
		Eval::BonaPiece piece() const { return piece_; }

		// 低次元の配列のindexを得る。ミラーしたものがkkp_[1]に返る。
		void toLowerDimensions(/*out*/ KKP kkp_[2]) const {
			kkp_[0] = KKP(king0_, king1_, piece_);
			kkp_[1] = KKP(Mir(king0_), Mir(king1_), mir_piece[piece_]);
		}

		// 現在のメンバの値に基いて、直列化されたときのindexを取得する。
		u64 toIndex() const {
			return min_index() + ((u64)king0_ * (u64)SQ_NB + (u64)king1_) * (u64)Eval::fe_end + (u64)piece_;
		}

	private:
		Square king0_, king1_;
		Eval::BonaPiece piece_;
	};

	struct KPP
	{
		KPP() {}
		KPP(Square king, Eval::BonaPiece p0, Eval::BonaPiece p1) : king_(king), piece0_(p0), piece1_(p1) {}

		// KK,KKP,KPP配列を直列化するときの通し番号の、KPPの最小値、最大値。
		static u64 min_index() { return KKP::max_index(); }
		static u64 max_index() { return min_index() + (u64)SQ_NB*(u64)Eval::fe_end*(u64)Eval::fe_end; }

		// 与えられたindexが、min_index()以上、max_index()未満にあるかを判定する。
		static bool is_ok(u64 index) { return min_index() <= index && index < max_index(); }

		// indexからKPPのオブジェクトを生成するbuilder
		static KPP fromIndex(u64 index)
		{
			index -= min_index();
			Eval::BonaPiece piece1 = (Eval::BonaPiece)(index % Eval::fe_end);
			index /= Eval::fe_end;
			Eval::BonaPiece piece0 = (Eval::BonaPiece)(index % Eval::fe_end);
			index /= Eval::fe_end;
			Square king = (Square)(index  /* % SQ_NB */);
			ASSERT_LV3(king < SQ_NB);
			return KPP(king, piece0, piece1);
		}

		// fromIndex()を用いてこのオブジェクトを構築したときに、以下のアクセッサで情報が得られる。
		Square king() const { return king_; }
		Eval::BonaPiece piece0() const { return piece0_; }
		Eval::BonaPiece piece1() const { return piece1_; }

		// 低次元の配列のindexを得る。p1,p2を入れ替えたもの、ミラーしたものなどが返る。
		void toLowerDimensions(/*out*/ KPP kpp_[4]) const {
			kpp_[0] = KPP(king_, piece0_, piece1_);
			kpp_[1] = KPP(king_, piece1_, piece0_);
			kpp_[2] = KPP(Mir(king_), mir_piece[piece0_], mir_piece[piece1_]);
			kpp_[3] = KPP(Mir(king_), mir_piece[piece1_], mir_piece[piece0_]);
		}

		// 現在のメンバの値に基いて、直列化されたときのindexを取得する。
		u64 toIndex() const {
			return min_index() + ((u64)king_ * (u64)Eval::fe_end + (u64)piece0_) * (u64)Eval::fe_end + (u64)piece1_;
		}

	private:
		Square king_;
		Eval::BonaPiece piece0_, piece1_;
	};

}

#endif
