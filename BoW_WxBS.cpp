//#pragma once
#include "BoW_WxBS.hpp"
#include "iindex.hpp"
#include <iostream>
#include <fstream>
#include "cv.hpp"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "src/generic-driver.h"
#include "vl/generic.h"
#include "vl/sift.h"
#include "vl/mathop.h"
#include <vl/kmeans.h>
#include <vl/kdtree.h>
#include "detectors/structures.hpp"
#include "WxBSdet_desc.cpp"
#include "matching/matching.hpp"
#include "correspondencebank.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <ctime>       /* time */
#define nontest
using namespace std;
using namespace cv;
namespace BoW{
        struct greater
    {
        template<class T>
        bool operator()(T const &a, T const &b) const { return a > b; }
    };
    VL_INLINE void transpose_descriptor (vl_sift_pix* dst, vl_sift_pix* src)
    {
    int const BO = 8 ;  /* number of orientation bins */
    int const BP = 4 ;  /* number of spatial bins     */
    int i, j, t ;

    for (j = 0 ; j < BP ; ++j) {
        int jp = BP - 1 - j ;
        for (i = 0 ; i < BP ; ++i) {
        int o  = BO * i + BP*BO * j  ;
        int op = BO * i + BP*BO * jp ;
        dst [op] = src[o] ;
        for (t = 1 ; t < BO ; ++t)
            dst [BO - t + op] = src [t + o] ;
        }
    }
    }
    /*void LoadRegions(ImageRepresentation ImgRep,vector<nodes> d){
        //ImgRep.Name = 
        AffineRegionVector desc_regions;

        for(int i=0;i<d.size();i++){
            AffineRegion ar = d[i].region;
            desc_regions.push_back(ar);
        }
        ImgRep.AddRegions(desc_regions,"1","1");
    }*/
    int BagOfWords_WxBS::findCorrespondFeatures(vector<nodes> d1,vector<nodes>d2, vector<nodes> &out1,vector<nodes> &out2,multimap<int,int> matchlist){
        //find corresponding Feautres from d1 to d2
        int cnt=0;
        
        //query: i-th feature, j-th kdtree index ==  DB: multimap [j-th index] = i-th feature 
        for(int i=0; i<d1.size();i++){
            // d1[i].bin.index  is  j-th kdtree index
            int featureIndex = d1[i].bin[0].index;
            multimap<int, int>::iterator iter;
           
            //duplicate d1's some feature if that corrsponding feature of d2 is non-single 
            if(matchlist.count(featureIndex)>1){
                
                pair<map<int, int>::iterator, map<int, int>::iterator> iter_pair;
                
                iter_pair = matchlist.equal_range(featureIndex);
                //iter = matchlist.find(featureIndex);
                for (iter = iter_pair.first; iter != iter_pair.second; ++iter){
                    
                    int corrIndex = iter->second;

                    out1.push_back(d1[i]);
                    out2.push_back(d2[corrIndex]);
                    cnt++;
                }
            }else if(matchlist.count(featureIndex)==1){
                
                iter = matchlist.find(featureIndex);
                int corrIndex = iter->second;
                out1.push_back(d1[i]);
                out2.push_back(d2[corrIndex]);
                cnt++;
            }
        }
        
        return cnt;
    }
    double BagOfWords_WxBS::calScore(vector<nodes> d1,vector<nodes> d2, double* H){
        double score=0;
        double score_=0;
        //cout << H[0]<<" " <<H[1]<<" " <<H[2]<<endl;
        //cout << H[3]<<" " <<H[4]<<" " <<H[5]<<endl;
        //cout << H[6]<<" " <<H[7]<<" " <<H[8]<<endl;
        if(H[0]==-1||isnan(abs(H[0]))){
            score=-1;
            return score;
        }
        int n = d1.size();
        for(int i=0;i< n;i++){
            double x1 = d1[i].region.reproj_kp.x;
            double y1 = d1[i].region.reproj_kp.y;
            double x2 = d2[i].region.reproj_kp.x;
            double y2 = d2[i].region.reproj_kp.y;
            double x_ = (x1*H[0]+y1*H[1]+H[2])/(H[6]+H[7]+H[8]);
            double y_ = (x1*H[3]+y1*H[4]+H[5])/(H[6]+H[7]+H[8]);
            double distance = sqrt(pow((x2-x_),2)+pow((y2-y_),2));
            score += distance;
            score_+= x1*(x2*H[0]+y2*H[1]+H[2])+y1*(x2*H[3]+y2*H[4]+H[5])+(x2*H[6]+y2*H[7]+H[8]);
        }
        //cout<<"score: "<<score/double(n)<<endl;
        //cout<<"score_: "<<score_/double(n)<<endl;
        score = abs(score/double(n));
        if(isnan(score)){
            score = -1;
        }else cout<<"detected hypotheses"<<endl;
        return score;
    }

    void BagOfWords_WxBS::imageSearchUsingBoW(string I,int topn){
        // Returns the top matches to I from the inverted index 'iindex' computed
        // using bow_buildInvIndex
        // Uses TF-IDF based scoring to rank
        // config has following flags
        // config.geomRerank = m => do geometric reranking for top m results
        // config.topn = n => output top 'n' results (defaults to 10)
        // config.saveMatchesImageDir = 'dir/' => store the matches images in the
        // dir
        // @return : imgPaths is the list of image paths in ranked order
        // @return scores : is the corresponding scores - tf-idf in general, or
        // number of inliers (of top m images) if doing geometric reranking
        // @return all_matches : Only returned if the config.geomRerank = 1. A
        // cell array with {i} element = matches of I with the i^th image
        
        int query_num;
        vector<nodes> d = computeImageRep(I, query_num,0);
        
        cout<< "Tf-Idf..."<<endl;
        cout<<"query image descriptorn: "<<query_num<<endl;
        int Ni=0;
        int N=index.numImgs;
        int num = models.vocabSize;
        Mat tfIdf = Mat::zeros(Size(num,N),CV_32FC1);

        

        //count n_id
        for(int y=0;y<N;y++){
            //cout<<index.imgPath2id[y]<<endl;
            for(int x=0;x<200;x++){
                //cout <<tfIdf.at<double>(y,x)<<" ";
                if(0<=d[x].bin[0].index && d[x].bin[0].index<num){
                    if(index.vw2imgsList[y].count(d[x].bin[0].index)){
                        tfIdf.at<float>(y,d[x].bin[0].index)+=index.vw2imgsList[y][d[x].bin[0].index];
                        //tfIdf.at<double>(y,x)+=index.vw2imgsList[y][d[x].index];
                    }
                }
                //cout <<index.vw2imgsList[y][d[x].index]<<" ";
                //cout <<tfIdf.at<double>(y,x)<<" ";
            }
            //cout<<endl<<endl;
        }
        //for(int y=0;y<N;y++){
            //cout<<index.imgPath2id[y]<<endl;
            //for(int x=0;x<num;x++){
                //cout<<d[x].index<<" ";
                //cout <<tfIdf.at<double>(y,x)<<" ";
            //}
            //cout<<endl<<endl;
        //}

        //count n_d and calculate tf
        /* 
        for(int y=0;y<N;y++){
            for(int x=0;x<num;x++){
                //cout <<tfIdf.at<double>(y,x)<<" ";
                //if(tfIdf.at<double>(y,x)>0){
                    tfIdf.at<float>(y,x) /= float(index.vw2imgsList[y][-1]);
                    //cout <<tfIdf.at<double>(y,x)<<" ";
                //}
            }
        }
       //cout <<double(index.vw2imgsList[1][-1])<<" ";
      
        //calculate tf-idf by multiplying idf gotten
        for(int x=0;x<num;x++){
            if(0<=d[x].index && d[x].index<num){
                Ni=0;
                for(int y=0;y<N;y++){
                    if(index.vw2imgsList[y].count(d[x].index)){
                        Ni++;
                    }
                }
                //cout<<"x word: "<<x <<"th,  "<< d[x].index<< " NI: "<<Ni<<endl;
                for(int y=0;y<N;y++){
                    if(Ni>0)
                        if(tfIdf.at<float>(y,x)>0.){
                            tfIdf.at<float>(y,x)*=log10(float(N)/float(Ni));
                        }
                    else
                        tfIdf.at<float>(y,x)*=0;
                }
            }
        }

        for(int y=0;y<N;y++){
            cout<<index.imgPath2id[0]<<endl;
            for(int x=0;x<num;x++){
                cout <<tfIdf.at<double>(0,x)<<" ";
            }
            cout<<endl<<endl;
        }
        */
        //for efficiency to sort, exchange the indeces of img and sum, i.e. int,double -> double, int
        vector<pair<float,int>> score;
        //sum all of elemnts to score
        for(int y=0;y<N;y++){
            float sum=0.;
            for(int x=0;x<num;x++){
                //if(tfIdf.at<double>(y,x)>0.){
                    //cout<<tfIdf.at<double>(y,x)<<" ";
                    sum += tfIdf.at<float>(y,x);
                //}
            }
            //cout<<endl<<endl;
            //cout<<sum<<endl;
            score.push_back(make_pair(sum,y));
        }
        //sort
        sort(score.begin(), score.end(),greater());
        for(int i=0;i<topn;i++)
            cout<<"top "<<i<<" |score: "<<score[i].first<<" | at "<<index.imgPath2id[score[i].second]<<endl;

        //for(int mm=0;mm<query_num;mm++){
                //free(d[mm].bin);
            //}
            //free(d);
    }

    void BagOfWords_WxBS::imageSearchUsingWxBSMatcher(string I,int topn){
        vector<pair<double,int>> score;
        int query_num;
        int N=index.numImgs;
        //ImageRepresentation ImgRep1,ImgRep2;
        
        
        int VERB = Config1.OutputParam.verbose;

        vector<nodes> d1 = computeImageRep(I, query_num,0);
     
        //LoadRegions(ImgRep1,d1); //TODO

        int numPossbleRankingImgs=0;
        //1. find matching lists
        //query: i-th feature, j-th kdtree index ==  DB: multimap [j-th index] = i-th feature      
        for(int y=0;y<N;y++){
            //TODO have to know how Imgrep is constructed
            vector<nodes> out1,out2;
            CorrespondenceBank Tentatives;
            map<string, TentativeCorrespListExt> tentatives, verified_coors;

            int corrnum = findCorrespondFeatures(d1,regionVector[y],out1,out2,index.matchlist[y]);
            //cout<<"corrnum: "<<corrnum<<endl;
            
            if(corrnum > 8){
               
                //if(out1.size() != d1.size()){
                //    LoadRegions(ImgRep1,out1);
                //}
                //LoadRegions(ImgRep2,out2);


                //TODO: convert to TentativeCorrespListExt
                TentativeCorrespListExt current_tents;
                //AffineRegionVector tempRegs1=imgrep1.GetAffineRegionVector("1","1");
                //AffineRegionVector tempRegs2=imgrep2.GetAffineRegionVector("1","1");
                //cout<<"out2.size(): "<<out2.size()<<endl;
                
                for(int i=0;i<out2.size();i++){
                    TentativeCorrespExt tmp_corr;
                    //if(out1.size() != d1.size()){
                    //    tmp_corr.first = d1[i];
                    //}else
                     tmp_corr.first = out1[i].region;
                    tmp_corr.second = out2[i].region;

                    //cout << out1[i].region.reproj_kp.x<<", "<<out1[i].region.reproj_kp.y<<endl;
                    //cout << out2[i].region.reproj_kp.x<<", "<<out2[i].region.reproj_kp.y<<endl;
                    current_tents.TCList.push_back(tmp_corr);
                }
                //Tentatives.AddCorrespondences(current_tents,"1","1");
                
                //tentatives["All"] = Tentatives.GetCorresponcesVector("1","1");
                tentatives["All"]=current_tents;
                DuplicateFiltering(tentatives["All"], Config1.FilterParam.duplicateDist,Config1.FilterParam.mode);
      
            //2. matching using WxBS Matcher : geometric verification
                //duplicate filtering
                    /*if (Config1.FilterParam.doBeforeRANSAC) //duplicate before RANSAC
                    {
                    if (VERB) std::cerr << "Duplicate filtering before RANSAC with threshold = " << Config1.FilterParam.duplicateDist << " pixels." << endl;
                    DuplicateFiltering(tentatives["All"], Config1.FilterParam.duplicateDist,Config1.FilterParam.mode);
                    if (VERB) std::cerr << tentatives["All"].TCList.size() << " unique tentatives left" << endl;
                    }
                    curr_matches=log1.TrueMatch1st;

                    log1.Tentatives1st = tentatives["All"].TCList.size();
                    curr_start = getMilliSecs();
                    */
                log1.Tentatives1st = tentatives["All"].TCList.size();
                //ransac(lo-ransac like degensac) with LAF check
                //if (VERB) std::cerr << "LO-RANSAC(epipolar) verification is used..." << endl;
                //cout<<"log1.Tentatives1st: "<<log1.Tentatives1st<<endl;

                if(log1.Tentatives1st>8){
                    log1.TrueMatch1st =  LORANSACFiltering(tentatives["All"],
                                                        verified_coors["All"],
                                                        verified_coors["All"].H,
                                                        Config1.RANSACParam);
                    log1.InlierRatio1st = (double) log1.TrueMatch1st / (double) log1.Tentatives1st;

                //3. get score using L2 norm
                    // all of featurs convert using verified_coors["All"].H
                    //and scoring by distance
                    if(verified_coors["All"].H[0] != -1){
                        double score_ =0;
                        
                        //TODO: need to remove duplicate points
                        score_= calScore(out1,out2,verified_coors["All"].H);
                        if(score_!=-1){
                            score.push_back(make_pair(score_,y));
                            numPossbleRankingImgs++;
                        }
                    
                    }
                }
            }

        }
        if (numPossbleRankingImgs < topn) topn = numPossbleRankingImgs;
        
        //sort
        sort(score.begin(), score.end());
        for(int i=0;i<topn;i++)
            cout<<"top "<<i<<" |score: "<<score[i].first<<" | at "<<index.imgPath2id[score[i].second]<<endl;

        //for(int mm=0;mm<num;mm++){
        //        free(d[mm].bin);
        //    }
    }


    vector<nodes> BagOfWords_WxBS::computeImageRep(string I,int &num, int flag){
        // Computes an image representation (of I) using the quantization 
        // parameters in the model
        // @param I : image after imread
        // @param model : model as generated by bow_computeVocab
        // @return : f (Same as from vl_sift) and bins = quantized descriptor values
        vector<AffineRegion> RootSIFTregion;
        vector<AffineRegion> HalfRootSIFTregion;
        //vl_sift_set_peak_thresh(sift,3);
        
        int i=0;
        WxBSdet_desc(I, RootSIFTregion,HalfRootSIFTregion);

        int Rsizevec = int(RootSIFTregion.size());
        //float* Rdesc;
        //Rdesc = (float*)vl_malloc(128*Rsizevec*sizeof(float));
        num = Rsizevec;
            
        //for(int p=0;p<Rsizevec*128;p++){
            
            //Rdesc[p] = RootSIFTdesc[p];
            
        //}
        clock_t begin = clock();
        //nodes* binlist = (nodes*)malloc(Rsizevec*sizeof(nodes));
        vector<nodes> binlist;
        cout<<Rsizevec<<endl;
        
        if (flag == 1)
        for(int i=0;i<Rsizevec;i++){
            nodes a;
            a.region = RootSIFTregion[i];
            //cout<<"11"<<endl;
            int len= RootSIFTregion[i].desc.vec.size();
            float* desc = (float*)vl_malloc(len*sizeof(float));
            //cout<<"len:"<<len<<endl;
            for(int j=0;j<len;j++)
                desc[j] = RootSIFTregion[i].desc.vec[j];
            //cout<<"111"<<endl;
            vl_kdforest_query(models.RootSIFTkdtree,a.bin,1,desc);
            //cout<<i<<" ";
            //cout<<"1111"<<endl;
            //binlist[i] = a;
            binlist.push_back(a);
            //cout<<"11111"<<endl;
            //binvec.push_back(a);
            free(desc);
        }
        else if (flag == 0){ /*for WxBS matcher*/
        int rand_=0;
        srand ((unsigned int)time(NULL));
        /*random sampling*/
        for(int i=0;i<100;i++){

            
            rand_ = rand() % 100;
            //rand_ = i;
            //cout<<"rand: "<<rand_<<endl;
            nodes a;
            a.region = RootSIFTregion[rand_];
            //cout << a.region.reproj_kp.x<<", "<<a.region.reproj_kp.y<<endl;
            //cout<<"11"<<endl;
            int len= RootSIFTregion[rand_].desc.vec.size();
            float* desc = (float*)vl_malloc(len*sizeof(float));
            //cout<<"len:"<<len<<endl;
            for(int j=0;j<len;j++)
                desc[j] = RootSIFTregion[rand_].desc.vec[j];
            //cout<<"111"<<endl;
            vl_kdforest_query(models.RootSIFTkdtree,a.bin,1,desc);
            //cout<<i<<" ";
            //cout<<"1111"<<endl;
            //binlist[i] = a;
            binlist.push_back(a);
            //cout<<"11111"<<endl;
            //binvec.push_back(a);
            free(desc);
            }
        }
        //cout<<"2"<<endl;

        clock_t end = clock();  
        double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
        cout<<"time: "<<elapsed_secs<<endl;

        //free(fdata);
        //free(Rdesc);
        //vl_kdforestsearcher_delete(quary);
        
        return binlist;
    }
    void BagOfWords_WxBS::buildInvIndex(string imgsDir,int numImg,int flag){
        vector<multimap<int,int>> matchlist;
        //vector<map<int,int>> vw2imgsList;
        //vw2imgsList.reserve(models.vocabSize);
        //vw2imgsList.reserve(index.numImgs);
        //for i = 1 : model.vocabSize
        //    vw2imgsList{i} = containers.Map('KeyType', 'int64', 'ValueType', 'int64');
        //end
        index.dirname = imgsDir;
        string frpaths;
        fstream f;
        f.open(imgsDir);
        index.numImgs = numImg;
        
        for(int i=0;i<index.numImgs;i++){
            int num=0;
            //map<int,int> init;
            multimap<int,int> init_;
            //vw2imgsList.push_back(init);
            matchlist.push_back(init_);

            if(flag==1){
                f>>frpaths;
                string path = "/home/jun/ImageDataSet/trainImg/"+frpaths;
                //cout<< path<<" "<<i<<"th image"<<endl;
                //cout<<path<<endl;
                //index.imgPaths.push_back(path);
                //// Get imgs list
                // Add these paths to a hash map as well
                index.imgPath2id.insert(pair<int,string>(i,path));
            }

        //for i = 1 : index.numImgs
            //try{
                //Mat I = imread(index.imgPath2id[i],0);
                //resize(I, I, Size(640,480));
                string I = index.imgPath2id[i];
                vector<nodes> d = computeImageRep(I, num,1);
                regionVector.push_back(d);
                    //for(int x=10000;x<15000;x++){
                    //    cout<<d[x].bin[0].index<<" ";
                    //}
                    //cout<<endl;
                
                //[~, d] = bow_computeImageRep(I, model, 'PeakThresh', 3);//TODO conversion
                //return descriptor vectors
///*
                //index.totalDescriptors[i] = d.size();
                for(int j=0; j<num;j++){
                //for(int j=0; j<models.vocabSize;j++){
                //for j = 1 : numel(d)
                    //map<int,int> imgsList = vw2imgsList{d(j)};
                    //map<int,int> imgsList = vw2imgsList[d[j].index];
                    //map<int,int> imgsList = vw2imgsList[i];
                    
                    
                    matchlist[i].insert(pair<int, int>(d[j].bin[0].index, j));
                    
                /*    
                    if (vw2imgsList[i].count(d[j].bin[0].index)){
                        vw2imgsList[i][d[j].bin[0].index] += 1;
                        
                    }else{
                        vw2imgsList[i][d[j].bin[0].index] = 1;
                        
                    }
                
                }
                vw2imgsList[i].insert(pair<int,int>(-1,num));*/
                }
                //cout<<"size: "<<vw2imgsList[i].size()<<endl;
                //vl_free(d);
            //}
            //catch(int e){
            //    cerr<<"the error in making inverted index"<<endl;
                //disp(getReport(e));
            //    continue;
            //}
            //end
            
            //if (i% 1000 == 0){
            //    index.vw2imgsList = vw2imgsList;
            //}
//*/

            //for(int y=0;y<1;y++){
                //cout<<index.imgPath2id[y]<<endl;
                //for(int x=0;x<models.vocabSize;x++){
                    //cout<<vw2imgsList[i][x]<<" ";
                    //;
                //}
                //cout<<endl;
            //}

            printf("nFeat = %d. Indexed (%d / %d)\n", num, i+1, index.numImgs);

            for(int mm=0;mm<num;mm++){
                free(d[mm].bin);
            }
            //free(d);

        }
        //index.vw2imgsList = vw2imgsList;
        index.matchlist = matchlist;
        


        f.close();
         

        //if 1
        //    fprintf('Saving to %s after %d files\n', fullfile(imgsDir, 'iindex.mat'), i);
        //    save(fullfile(imgsDir, 'iindex.mat'), 'iindex', '-v7.3');
        //end
    }


    int BagOfWords_WxBS::computeVocab(string imgsDir, int numImg){

        index.dirname = imgsDir;
        //index.totalDescriptors.reserve(index.numImgs);
        int avgSiftsPerImg = 1000;
        
        int est_n = avgSiftsPerImg*56; // expected number of sifts


        // Read images and create set of SIFTs
        //Mat toFloat; 
        //cvmat.convertTo(toFloat,CV_32F);
        //float *vlimage = (float*) tofloat.data;
        
        //descs = zeros(128, est_n, 'uint8'); // 128 x n dim matrix, for n SIFTs
        int found_sifts = 0;
        cout<<"Reading SIFTs "<<endl;
        /*
        for i = 1 : numel(fullpaths)
            // best to read one by one, in case of large number of images
            try
                I = single(rgb2gray(imread(fullpaths{i})));
                I = imresize(I,[640,480]);
            catch
                fprintf(2, 'Unable to read %s\n', fullpaths{i});
                continue;
            end
        */

        string frpaths;
        fstream f;
        f.open(imgsDir);
        int totalcnt=0;
        //[~, d] = vl_sift(I);
        //string config = "config_iter_mods_cviu_wxbs.ini";
        //string iters = "iters_mods_cviu_wxbs_2.ini";
        
        vector<float> RootSIFTdesc;
        vector<float> HalfRootSIFTdesc;
        int m=0;
        for (m = 0; m<numImg;m++){
            vector<AffineRegion> RootSIFTregion;
            vector<AffineRegion> HalfRootSIFTregion;

            f>>frpaths;
            string path = "/home/jun/ImageDataSet/trainImg/"+frpaths;
            cout<< path<<" "<<m+1<<"th image"<<endl;
            //cout<<path<<endl;
            //index.imgPaths.push_back(path);
            //// Get imgs list
            // Add these paths to a hash map as well
            index.imgPath2id.insert(pair<int,string>(m,path));

            int totalnKey=0;
            int i=0;
            int descNum=0;
            WxBSdet_desc(path, RootSIFTregion,HalfRootSIFTregion);

            

            for(int i=0;i<RootSIFTregion.size();i++)
                for (int ddd = 0; ddd <RootSIFTregion[i].desc.vec.size(); ++ddd){
                    RootSIFTdesc.push_back(RootSIFTregion[i].desc.vec[ddd]);
                    //kpfile << ar.desc.vec[ddd] << " ";
                    }


            cout<<"sizeof R SIFT desc: "<<RootSIFTregion.size()<<endl;
            
            //printf ("sift: detected %d (unoriented) keypoints\n", totalnKey) ;
            //printf ("sift: detected %d (unoriented) descriptors (%d)\n", descNum,m+1) ;
            //free(fdata);
            //free(currImg_data);
            
        }

        index.numImgs = numImg;
            //textprogressbar(i * 100.0 / numel(fullpaths));
        
        cout<<"Done"<<endl;
        //cout<<totalcnt<<endl;
        int Rsizevec = int(RootSIFTdesc.size())/128;
        cout<<"Rsize vec: "<<Rsizevec<<endl;
        float* Rdesc;
        Rdesc = (float*)vl_malloc(Rsizevec*128*sizeof(float));
     
        for(int p=0;p<Rsizevec*128;p++){
            //for(int j=0;j<128;j++){
            Rdesc[p] = RootSIFTdesc[p];
            //Rdesc[p*128+j] = RootSIFTdesc[p*128+j];
            //cout<< Rdesc[p]<<" ";
            //}
            //cout<<endl;
        }
        /*
        int HRsizevec = int(HalfRootSIFTdesc.size());
        cout<<"HRsize vec: "<<HRsizevec<<endl;
        float* HRdesc;
        HRdesc = (float*)vl_malloc(64*HRsizevec*sizeof(float));

            
        for(int p=0;p<HRsizevec;p++){
            for(int j=0;j<64;j++){
            
            HRdesc[p*64+j] = HalfRootSIFTdesc[p*64+j];
            
            }
            //cout<<endl;
        }
        */
        f.close();
        cout<< "Found RootSIFT " <<Rsizevec<<" descriptors. "<<endl;
        //cout<< "Found HlafRootSIFT " <<HRsizevec<<" descriptors. "<<endl;
        // K Means cluster the SIFTs, and create a model
        models.vocabSize = min(Rsizevec, params.numWords);
        //vl_file_meta_close (&dsc) ;
        cout<<"clustering RootSIFTdesc..."<<endl;
        vl_size numData = Rsizevec;
        vl_size dimension = 128;
        vl_size numCenters = min(Rsizevec, params.numWords);
        vl_size maxiter = 100;
        vl_size maxComp = 100;
        vl_size maxrep = 1;
        vl_size ntrees = 3;
        
        vl_kmeans_set_verbosity	(kmeans,1);
        // Use Lloyd algorithm
        vl_kmeans_set_algorithm(kmeans, VlKMeansANN) ;
        vl_kmeans_set_max_num_comparisons (kmeans, maxComp) ;
        vl_kmeans_set_num_repetitions (kmeans, maxrep) ;
        vl_kmeans_set_num_trees (kmeans, ntrees);
        //vl_sift_pix const* c_desc = desc;
        vl_kmeans_set_max_num_iterations (kmeans, maxiter) ;
        // Initialize the cluster centers by randomly sampling the data
        vl_kmeans_init_centers_plus_plus (kmeans, Rdesc, dimension, numData, numCenters) ;
        // Run at most 100 iterations of cluster refinement using Lloyd algorithm
        
        //vl_kmeans_refine_centers (kmeans, desc, numData) ;
        vl_kmeans_cluster(kmeans,Rdesc,dimension,numData,numCenters);

        
        vl_kdforest_build(models.RootSIFTkdtree,numCenters,kmeans->centers);
    /*
        cout<<"clustering HalfRootSIFTdesc..."<<endl;
        vl_size numData = HRsizevec;
        vl_size dimension = 64;
        vl_size numCenters = min(HRsizevec, params.numWords);
        vl_kmeans_init_centers_plus_plus (kmeans, HRdesc, dimension, numData, numCenters) ;
        vl_kmeans_cluster(kmeans,HRdesc,dimension,numData,numCenters);
        vl_kdforest_build(models.HalfRootSIFTkdtree,numCenters,kmeans->centers);
*/
        vl_free(Rdesc);
        RootSIFTdesc.clear();
        HalfRootSIFTdesc.clear();
        //vl_free(HRdesc);
        return 0;

        //descs = descs(:, 1 : found_sifts);
        
/*
        
        VlKDForest* kdtree =  vl_kdforest_new(VL_TYPE_FLOAT,128,numTrees,VlDistanceL2);
        vl_kdforest_build(kdtree,numData,data);
        models.kdtrees= kdtree;

        vl_kdforest_delete(kdtree);
        */
       
        //model.vocab = vl_kmeans(double(descs), ...
        //                        min(size(descs, 2), params.numWords), 'verbose', ...
        //                        'algorithm', 'ANN');
        //model.kdtree = vl_kdtreebuild(model.vocab);
            
    }

    void BagOfWords_WxBS::saveVocab(string name){
        cout<<"saving vocabulary..."<<endl;
        ofstream f(name);

        float *vocab = (float*)malloc(models.vocabSize*128*sizeof(float));
        vocab = (float*)kmeans->centers;
        for(int i=0;i<models.vocabSize*128;i++){
            f<<vocab[i]<<" ";
        }
        f.close();
        //free(vocab);
    }
    void BagOfWords_WxBS::loadVocab(string name){
        cout<<"loading vocabulary..."<<endl;
        fstream f;
        f.open(name);
         float *vocab = (float*)malloc(models.vocabSize*128*sizeof(float));
        for(int i=0;i<models.vocabSize*128;i++){
            f>>vocab[i];
        }
        models.vocabSize = params.numWords;
        vl_kdforest_build(models.RootSIFTkdtree,models.vocabSize,vocab);
        cout<<"done"<<endl;
        //free(vocab);
        f.close();
    }

    void BagOfWords_WxBS::saveIndex(string name){
        cout<<"saving InvertedIndex..."<<endl;
        ofstream f(name);

        f<< index.dirname<<endl;

        //total image size and map length save
        f<< index.numImgs<<endl;




        map<int, string>::iterator iter;
        for (iter = index.imgPath2id.begin(); iter != index.imgPath2id.end(); ++iter)
            f<<iter->first<<" "<<iter->second<<endl;
        
        vector<map<int, int>>::iterator vec;
        map<int, int>::iterator iter_;
        int i=0;
        //for(vec = index.vw2imgsList.begin(); vec != index.vw2imgsList.end(); i++,++vec){
        for(i=0;i<index.numImgs;i++){    
            /*
            f<<vec->size()<<endl;
            
            int k=0;
            
            for (iter_ = vec->begin(); iter_ != vec->end(); ++iter_)
                    f<<iter_->first<<" "<<iter_->second<<" ";
            //k++;
            f<<endl;
            */


            f<<regionVector[i].size()<<endl;
            //keyregion save
            for(int j=0;j<regionVector[i].size();j++){
                //f<<*regionVector[i][j].bin[0].index<<" ";
                f<<regionVector[i][j].region.img_id<<" "<<regionVector[i][j].region.img_reproj_id
                <<" "<<regionVector[i][j].region.id<<" "<<regionVector[i][j].region.parent_id
                //<<" "<<regionVector[i][j].region.type
                //<<" "<<regionVector[i][j].region.det_kp.x
                //<<" "<<regionVector[i][j].region.det_kp.y<<" "<<regionVector[i][j].region.det_kp.a11
                //<<" "<<regionVector[i][j].region.det_kp.a12<<" "<<regionVector[i][j].region.det_kp.a21
                //<<" "<<regionVector[i][j].region.det_kp.a22<<" "regionVector[i][j].region.det_kp.s
                //<<" "<<regionVector[i][j].region.det_kp.response<<" "<<regionVector[i][j].region.det_kp.octave_number
                //<<" "<<regionVector[i][j].region.det_kp.pyramid_scale<<" "<<regionVector[i][j].region.det_kp.sub_type
                <<" "<<regionVector[i][j].region.reproj_kp.x
                <<" "<<regionVector[i][j].region.reproj_kp.y<<" "<<regionVector[i][j].region.reproj_kp.a11
                <<" "<<regionVector[i][j].region.reproj_kp.a12<<" "<<regionVector[i][j].region.reproj_kp.a21
                <<" "<<regionVector[i][j].region.reproj_kp.a22<<" "<<regionVector[i][j].region.reproj_kp.s
                <<" "<<regionVector[i][j].region.reproj_kp.response<<" "<<regionVector[i][j].region.reproj_kp.octave_number
                <<" "<<regionVector[i][j].region.reproj_kp.pyramid_scale<<" "<<regionVector[i][j].region.reproj_kp.sub_type;
                f<<endl;

                
            }
        }
        f<<index.matchlist.size()<<" ";
        //matchlist save
        for(i=0;i<index.matchlist.size();i++){
            f<<index.matchlist[i].size()<<" ";
            multimap<int, int>::iterator iter;  
            for(iter = index.matchlist[i].begin();iter!=index.matchlist[i].end();++iter){
                f<<iter->first<<" "<<iter->second<<" ";
            }
        }

        f.close();
    }
    void BagOfWords_WxBS::loadIndex(string name){
        cout<<"loading InvertedIndex..."<<endl;
        fstream f;
        f.open(name);
        models.vocabSize = params.numWords;
        f>>index.dirname;
        f>>index.numImgs;
        for(int i=0;i<index.numImgs;i++){
            int first;
            string second;

            f>>first>>second;
            index.imgPath2id.insert(pair<int,string>(first,second));
        }
        vector<vector<nodes>> nodevv;
        for(int i=0;i<index.numImgs;i++){
            
            int length;
            /*
            f>>length;
            map<int,int> init;
            index.vw2imgsList.push_back(init);
            for(int j=0;j<length;j++){
                int first,second;
                f>>first>>second;
                index.vw2imgsList[i].insert(pair<int,int>(first,second));
            }
*/
            f>>length;
            vector<nodes> nodev;
            for(int j=0;j<length;j++){
                nodes a;
                AffineRegion b;
                //f>>a.bin[0].index;
                f>>b.img_id>>b.img_reproj_id>>b.id;
                f>>b.parent_id;
                //f>>b.type;
                a.region = b;
                //>>a.region.det_kp.x>>a.region.det_kp.y
                //>>a.region.det_kp.a11>>a.region.det_kp.a12
                //>>a.region.det_kp.a21>>a.region.det_kp.a22
                //>>a.region.det_kp.s>>a.region.det_kp.response
                //>>a.region.det_kp.octave_number>>a.region.det_kp.pyramid_scale
                //>>a.region.det_kp.sub_type
                f>>a.region.reproj_kp.x>>a.region.reproj_kp.y;
                f>>a.region.reproj_kp.a11>>a.region.reproj_kp.a12;
                f>>a.region.reproj_kp.a21>>a.region.reproj_kp.a22;
                f>>a.region.reproj_kp.s>>a.region.reproj_kp.response;
                f>>a.region.reproj_kp.octave_number>>a.region.reproj_kp.pyramid_scale;
                f>>a.region.reproj_kp.sub_type;
                nodev.push_back(a);
            }
            nodevv.push_back(nodev);



            cout<<"done with "<< i+1<<"/"<<index.numImgs<<endl;
        }
        regionVector=nodevv;

        int len;
        f>>len;
        for(int i=0;i<len;i++){
            int len_;
            int first,second;
            f>>len_;
            
            multimap<int,int>a;
            for(int j=0;j<len_;j++){
                f>>first>>second;
                a.insert(pair<int, int>(first, second));
            } 
            index.matchlist.push_back(a);
        }

        f.close();
    }

}