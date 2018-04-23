#pragma once
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
#include "WxBSdet_desc.cpp"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
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

    void BagOfWords_WxBS::imageSearch(string I,int topn){
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
        int num;
        VlKDForestNeighbor* d = computeImageRep(I, num);
        
        cout<< "Tf-Idf..."<<endl;
        cout<<"query image descriptorn: "<<num<<endl;
        int Ni=0;
        int N=index.numImgs;

        Mat tfIdf = Mat::zeros(Size(num,N),CV_64FC1);

        //count n_id
        for(int y=0;y<N;y++){
            //cout<<index.imgPath2id[y]<<endl;
            for(int x=0;x<num;x++){
                
                if(index.vw2imgsList[y].count(d[x].index)){
                    
                    tfIdf.at<double>(y,x)+=index.vw2imgsList[y][d[x].index];
                }
                //cout <<tfIdf.at<double>(y,x)<<" ";
            }
            //cout<<endl<<endl;
        }
        //count n_d and calculate tf
        for(int y=0;y<N;y++){
            for(int x=0;x<num;x++){
                if(tfIdf.at<double>(y,x)>0.){
                    tfIdf.at<double>(y,x)/=double(index.vw2imgsList[y][-1]);
                }
            }
        }
        
        //calculate tf-idf by multiplying idf gotten
        for(int x=0;x<num;x++){
            Ni=0;
            for(int y=0;y<N;y++){
                if(index.vw2imgsList[y].count(d[x].index)){
                    Ni++;
                }
            }
            //cout<<"x word: "<<x <<"th,  "<< d[x].index<< " NI: "<<Ni<<endl;
            for(int y=0;y<N;y++){
                if(tfIdf.at<double>(y,x)>0.){
                    tfIdf.at<double>(y,x)*=log10(double(N)/double(Ni));
                }
            }
        }

        //for(int y=0;y<N;y++){
            //cout<<index.imgPath2id[9]<<endl;
            //for(int x=0;x<num;x++){
            //    cout <<tfIdf.at<double>(9,x)<<" ";
            //}
            //cout<<endl<<endl;
        //}

        //for efficiency to sort, exchange the indeces of img and sum, i.e. int,double -> double, int
        vector<pair<double,int>> score;
        //sum all of elemnts to score
        for(int y=0;y<N;y++){
            double sum=0.;
            for(int x=0;x<num;x++){
                if(tfIdf.at<double>(y,x)>0.){
                    sum += tfIdf.at<double>(y,x);
                }
            }
            score.push_back(make_pair(sum,y));
        }
        //sort
        sort(score.begin(), score.end(),greater());
        for(int i=0;i<topn;i++)
            cout<<"top "<<i<<" |score: "<<score[i].first<<" | at "<<index.imgPath2id[score[i].second]<<endl;
    }

    VlKDForestNeighbor* BagOfWords_WxBS::computeImageRep(string I,int &num){
        // Computes an image representation (of I) using the quantization 
        // parameters in the model
        // @param I : image after imread
        // @param model : model as generated by bow_computeVocab
        // @return : f (Same as from vl_sift) and bins = quantized descriptor values
        string config = "/home/jun/BOW_WxBS/config_iter_mods_cviu_wxbs.ini";
        string iters = "/home/jun/BOW_WxBS/iters_mods_cviu_wxbs_2.ini";
        vector<float> RootSIFTdesc;
        vector<float> HalfRootSIFTdesc;
        //vl_sift_set_peak_thresh(sift,3);
        
        int i=0;
        WxBSdet_desc(I, config, iters,RootSIFTdesc,HalfRootSIFTdesc);

        //index.totalDescriptors.push_back(descNum);
        int Rsizevec = int(RootSIFTdesc.size());
        //cout<<"size vec: "<<sizevec<<endl;
        float* Rdesc;
        Rdesc = (float*)vl_malloc(128*Rsizevec*sizeof(float));

            
        for(int p=0;p<Rsizevec;p++){
            for(int j=0;j<128;j++){
            
            Rdesc[p*128+j] = RootSIFTdesc[p*128+j];
            
            }
            //cout<<endl;
        }
        VlKDForestSearcher* quary= vl_kdforest_new_searcher(models.RootSIFTkdtree); 	
        VlKDForestNeighbor * bin =(VlKDForestNeighbor *)vl_malloc(Rsizevec*sizeof(VlKDForestNeighbor));
        
        vl_kdforestsearcher_query(quary,bin,Rsizevec,RootSIFTdesc);
        //vl_kdforest_query(models.kdtree,bin,models.vocabSize,desc);
        //free(fdata);
        free(Rdesc);
        return bin;
    }
    void BagOfWords_WxBS::buildInvIndex(string imgsDir){
        // Build an inverted index for all image files in 'imgsDir' (recursively
        // searched) given the visual word quantization 'model'
        // model can be path to mat file too
        // Optional Parameters:
        // 'imgsListFpath', 'path/to/file.txt' :- File contains a newline separated
        // 'resDir', 'path/to/results'
        // list of image paths (relative to imgsDir) of the image files to 
        // build index upon. Typically used to set the train set.
        //save iindex.mat
        //string indexfpath = "/home/jun/ImageDataSet/iindex.txt";
        //string indexfpath = fullfile(imgsDir, 'iindex.mat');
        //if exist(indexfpath, 'file')
            //fprintf(2, 'The iindex file already exists. Remove it first\n');
            //return;
        //end
        //index = matfile(indexfpath, 'Writable', true);

        //index.imgPath2id = containers.Map(index.imgPaths,1 : index.numImgs);
        //fullpaths = cellfun2(@(x) fullfile(imgsDir, x), index.imgPaths);

        //// create inverted index
        
        //index.totalDescriptors = zeros(index.numImgs, 1);
        // will store the total # of words in each image
        // Create a cell array of vocabSize containers.Map (Assuming vocab ids are
        // 1..n). Each element stores <imgID : times that VW appears in that image>
        // Have to call it multiple times to initialize in a loop.. using 
        // `repmat` or `deal` simply makes multiple references to same object and 
        // that doesn't work
        vector<map<int,int>> vw2imgsList;
        //vw2imgsList.reserve(models.vocabSize);
        vw2imgsList.reserve(index.numImgs);
        //for i = 1 : model.vocabSize
        //    vw2imgsList{i} = containers.Map('KeyType', 'int64', 'ValueType', 'int64');
        //end
        
        for(int i=0;i<index.numImgs;i++){
            int num=0;
            map<int,int> init;
            vw2imgsList.push_back(init);
        //for i = 1 : index.numImgs
            try{
                //Mat I = imread(index.imgPath2id[i],0);
                //resize(I, I, Size(640,480));
                string I = index.imgPath2id[i];
                VlKDForestNeighbor* d = computeImageRep(I, num);
                //[~, d] = bow_computeImageRep(I, model, 'PeakThresh', 3);//TODO conversion
                //return descriptor vectors
///*
                //index.totalDescriptors[i] = d.size();
                for(int j=0; j<num;j++){
                //for j = 1 : numel(d)
                    //map<int,int> imgsList = vw2imgsList{d(j)};
                    //map<int,int> imgsList = vw2imgsList[d[j].index];
                    map<int,int> imgsList = vw2imgsList[i];

                    //if (imgsList.count(i)>0)
                    if (imgsList.count(d[j].index))
                        imgsList[d[j].index] = imgsList[d[j].index]+1;
                        //imgsList(i) = imgsList(i) + 1;
                    else
                        imgsList[d[j].index] = 1;
                        
                    //vw2imgsList[d[j].index] = imgsList;
                    vw2imgsList[i] = imgsList;
                    //vw2imgsList{d(j)} = imgsList;
                //end
                }
                vw2imgsList[i].insert(pair<int,int>(-1,num));
            }
            catch(int e){
                cerr<<"the error in making inverted index"<<endl;
                //disp(getReport(e));
                continue;
            }
            //end
            printf("nFeat = %d. Indexed (%d / %d)\n", num, i+1, index.numImgs);
            //if (i% 1000 == 0){
            //    index.vw2imgsList = vw2imgsList;
            //}
//*/
        }
        index.vw2imgsList = vw2imgsList;


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
        string config = "/home/jun/BOW_WxBS/config_iter_mods_cviu_wxbs.ini";
        string iters = "/home/jun/BOW_WxBS/iters_mods_cviu_wxbs_2.ini";
        vector<float> RootSIFTdesc;
        vector<float> HalfRootSIFTdesc;

        int m=0;
        for (m = 0; m<numImg;m++){
            f>>frpaths;
            string path = "/home/jun/ImageDataSet/trainImg/"+frpaths;
            //cout<<path<<endl;
            index.imgPaths.push_back(path);
            //// Get imgs list
            // Add these paths to a hash map as well
            index.imgPath2id.insert(pair<int,string>(m,path));

            int totalnKey=0;
            int i=0;
            int descNum=0;
            WxBSdet_desc(path, config, iters,RootSIFTdesc,HalfRootSIFTdesc);

            
            //printf ("sift: detected %d (unoriented) keypoints\n", totalnKey) ;
            //printf ("sift: detected %d (unoriented) descriptors (%d)\n", descNum,m+1) ;
            //free(fdata);
            //free(currImg_data);
            
        }

        index.numImgs = numImg;
            //textprogressbar(i * 100.0 / numel(fullpaths));
        
        cout<<"Done"<<endl;
        //cout<<totalcnt<<endl;
        int Rsizevec = int(RootSIFTdesc.size());
        cout<<"Rsize vec: "<<Rsizevec<<endl;
        float* Rdesc;
        Rdesc = (float*)vl_malloc(128*Rsizevec*sizeof(float));

            
        for(int p=0;p<Rsizevec;p++){
            for(int j=0;j<128;j++){
            
            Rdesc[p*128+j] = RootSIFTdesc[p*128+j];
            
            }
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
        models.vocabSize = params.numWords;
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

    
}