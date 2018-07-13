//descriptor.h
/***************************************************************
������: CDescriptor
������	�����������������ͼ����һ���������
���룺
�����	
���ߣ�	zhwang
�ʼ���	zhwang@nlpr.ia.ac.cn
���ڣ�	06.12.30
����޸ģ�
���ԣ�	
***************************************************************/
#include "stdafx.h"
#include "cv.h"
#include "highgui.h"
#include "wzhlib.h"

typedef struct SCNo
{
	int		nNo1;									// ��1�������
	int		nNo2;									// ��2�������
} SCNo;

typedef struct SCPos
{
	int		nNo1;									// ��1�������
	int		nNo2;									// ��2�������
	double	dCoe1;
	double	dCoe2;
} SCPos;

void	descriptorFreeMemory();

class CDescriptor
{	
	//����
	public:
		double		m_fSigma;						//��˹�˲��߶�
	public:
		//ͼ����Ϣ
		double*		m_pImageData;					//ͼ������
		int			m_nWidth;						//ͼ��߶�
		int			m_nHeight;						//ͼ����
		int			m_nTotolPixels;					//ͼ����������
		
		//�ǵ���Ϣ
		int			m_nLineCount;					//ֱ������
		int			m_nTotolPts;					//����ֱ���ϵĵ��ܸ���
		int			m_szPtsCounts[nMaxLineCount];	//����ֱ���ϵ�ĸ���
		float		m_scalesForEachLine[2*nMaxLineCount];
		float		m_angleForEachLine[2*nMaxLineCount];
		double*		m_pLinePts;						//���λ����Ϣ
		
		//�ݶ�
		double*		m_pDxImage;						//dxͼ��
		double*		m_pDyImage;						//dyͼ��
		double*		m_pMagImage;					//�ݶȷ�ֵ
				
		//��������Ϣ
		float*		m_scDes;						//Std��������
		int			m_nDesDim;						//������ά��
		char*		m_pByValidFlag;					//��ǽǵ��Ƿ���Ч
		double*		m_pMainArc;						//ÿ��ֱ�ߵ�������

	//������Ա
	public:
		CDescriptor(double* pGrayData,int nWidth,int nHegiht,
					double* pLinePts,int inLineCounts,int szPtsCounts[],float scalesForEachLine[],float angleForEachLine[]);
		~CDescriptor();

		//����������
		void	ComputeLineDescriptor();
		
	private:
		void	InitializeLUT();
		void	getScaledSubRegionPoints(int block_width, int block_height, double dArc);
		void	getScaledSubRegionPointsZeroAngle(int block_width, int block_height);
		void	getWeightingTable(int block_width, int block_height, double scale);
		void	ComputeDescriptorByMatrix(double* pLineDes,double* pMatrix,int nD,int nValid);
		void	ComputeSubRegionProjection(double* pDesMatrix,float angle1,float angle2,int nCenterR,int nCenterC, double scale);
		void	ComputeSubRegionProjectionZeroScale(double* pSubRegionDes,double dMainArc,int nCenterR,int nCenterC);
		void	ComputeSubRegionProjectionLowerHalf(double* pSubRegionDes, float angle, int nCenterR, int nCenterC, double scale);
		void	ComputeSubRegionProjectionUpperHalf(double* pSubRegionDes, float angle, int nCenterR, int nCenterC, double scale);
		void	ComputeSubRegionProjectionMiddle(double* pSubRegionDes, float angle, int nCenterR, int nCenterC, double scale);

		//�����ڻ������
		double	ComputeLineDir(double* pLinePts,int nCount,double dDxAvg, double dDyAvg);
};
