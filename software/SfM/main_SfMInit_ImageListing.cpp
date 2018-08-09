
#include "i23dSFM/exif/exif_IO_EasyExif.hpp"

#include "i23dSFM/exif/sensor_width_database/ParseDatabase.hpp"

#include "i23dSFM/image/image.hpp"
#include "i23dSFM/stl/split.hpp"
#include "i23dSFM/numeric/numeric.h"

#include "i23dSFM/sfm/sfm.hpp"

#include "third_party/cmdLine/cmdLine.h"
#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <iomanip>

#define DISTANCE_THRESHOLD 50.0
#define LAND_HEIGHT 120.0

using namespace i23dSFM;
using namespace i23dSFM::cameras;
using namespace i23dSFM::exif;
using namespace i23dSFM::image;
using namespace i23dSFM::sfm;

/// Check that Kmatrix is a string like "f;0;ppx;0;f;ppy;0;0;1"
/// With f,ppx,ppy as valid numerical value
bool checkIntrinsicStringValidity(const std::string & Kmatrix, double & focal, double & ppx, double & ppy)
{
    std::vector<std::string> vec_str;
    stl::split(Kmatrix, ";", vec_str);
    if (vec_str.size() != 9)  {
        std::cerr << "\n Missing ';' character" << std::endl;
        return false;
    }
    // Check that all K matrix value are valid numbers
    for (size_t i = 0; i < vec_str.size(); ++i) {
        double readvalue = 0.0;
        std::stringstream ss;
        ss.str(vec_str[i]);
        if (! (ss >> readvalue) )  {
            std::cerr << "\n Used an invalid not a number character" << std::endl;
            return false;
        }
        if (i==0) focal = readvalue;
        if (i==2) ppx = readvalue;
        if (i==5) ppy = readvalue;
    }
    return true;
}

// Vec3 : latitude, longitude, altitude
// Return distance(m)
float calcDistanceByGPS(Vec3 v1, Vec3 v2)
{
    float x = (v1[0] - v2[0]) * 111000,
            y = (v1[1] - v2[1]) * 111000,
            z = (v1[2] - v2[2]);
    return sqrt(x * x + y * y);
}

//
// Create the description of an input image dataset for I23dSFM toolsuite
// - Export a SfM_Data file with View & Intrinsic data
//
int main(int argc, char **argv)
{
    CmdLine cmd;

    std::string sImageDir,
            sSemanticImgDir = "",
            sfileDatabase = "",
            sOutputDir = "",
            sKmatrix;

    int i_User_camera_model = PINHOLE_CAMERA_RADIAL3;

    bool b_Group_camera_model = true;

    bool bExifGpsInfo = false;

    double focal_pixels = -1.0;

    cmd.add( make_option('i', sImageDir, "imageDirectory") );
    cmd.add( make_option('s', sSemanticImgDir, "semantic segmentation image directory") );  // added by chenyu
    cmd.add( make_option('d', sfileDatabase, "sensorWidthDatabase") );
    cmd.add( make_option('o', sOutputDir, "outputDirectory") );
    cmd.add( make_option('f', focal_pixels, "focal") );
    cmd.add( make_option('k', sKmatrix, "intrinsics") );
    cmd.add( make_option('c', i_User_camera_model, "camera_model") );
    cmd.add( make_option('g', b_Group_camera_model, "group_camera_model") );
    cmd.add( make_option('e', bExifGpsInfo, "exif_gps_info") );

    try {
        if (argc == 1) throw std::string("Invalid command line parameter.");
        cmd.process(argc, argv);
    } catch(const std::string& s) {
        std::cerr << "Usage: " << argv[0] << '\n'
                  << "[-i|--imageDirectory]\n"
                  << "[-s|--semantic segmentation image directory]\n"
                  << "[-d|--sensorWidthDatabase]\n"
                  << "[-o|--outputDirectory]\n"
                  << "[-f|--focal] (pixels)\n"
                  << "[-k|--intrinsics] Kmatrix: \"f;0;ppx;0;f;ppy;0;0;1\"\n"
                  << "[-c|--camera_model] Camera model type:\n"
                  << "\t 1: Pinhole\n"
                  << "\t 2: Pinhole radial 1\n"
                  << "\t 3: Pinhole radial 3 (default)\n"
                  << "\t 4: Pinhole brown 2\n"
                  << "[-g|--group_camera_model]\n"
                  << "\t 0-> each view have it's own camera intrinsic parameters,\n"
                  << "\t 1-> (default) view can share some camera intrinsic parameters\n"
                  << "[-e|--exif_gps_info]\n"
                  << std::endl;

        std::cerr << s << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << " You called : " <<std::endl
              << argv[0] << std::endl
              << "--imageDirectory " << sImageDir << std::endl
              << "--semantic segmentation image directory " << sSemanticImgDir << std::endl
              << "--sensorWidthDatabase " << sfileDatabase << std::endl
              << "--outputDirectory " << sOutputDir << std::endl
              << "--focal " << focal_pixels << std::endl
              << "--intrinsics " << sKmatrix << std::endl
              << "--camera_model " << i_User_camera_model << std::endl
              << "--group_camera_model " << b_Group_camera_model << std::endl
              << "--exif_gps_info " << bExifGpsInfo << std::endl;

    // Expected properties for each image
    double width = -1, height = -1, focal = -1, ppx = -1,  ppy = -1;

    const EINTRINSIC e_User_camera_model = EINTRINSIC(i_User_camera_model);

    if ( !stlplus::folder_exists( sImageDir ) )
    {
        std::cerr << "\nThe input directory doesn't exist" << std::endl;
        return EXIT_FAILURE;
    }

    if( !stlplus::folder_exists( sSemanticImgDir ) )
    {
        std::cerr << "\nThe input semantic segmentation directory doesn't exist" << std::endl;
    }

    if (sOutputDir.empty())
    {
        std::cerr << "\nInvalid output directory" << std::endl;
        return EXIT_FAILURE;
    }

    if ( !stlplus::folder_exists( sOutputDir ) )
    {
        if ( !stlplus::folder_create( sOutputDir ))
        {
            std::cerr << "\nCannot create output directory" << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (sKmatrix.size() > 0 &&
        !checkIntrinsicStringValidity(sKmatrix, focal, ppx, ppy) )
    {
        std::cerr << "\nInvalid K matrix input" << std::endl;
        return EXIT_FAILURE;
    }

    if (sKmatrix.size() > 0 && focal_pixels != -1.0)
    {
        std::cerr << "\nCannot combine -f and -k options" << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<Datasheet> vec_database;
    if (!sfileDatabase.empty())
    {
        if ( !parseDatabase( sfileDatabase, vec_database ) )
        {
            std::cerr
                    << "\nInvalid input database: " << sfileDatabase
                    << ", please specify a valid file." << std::endl;
            return EXIT_FAILURE;
        }
    }

    // extract an array of filenames (not paths) of all the files found in the specified folder.
    // each of these names can be combined with folder and form _filespec() to give the 
    // filespec of each file.
    // More details: see http://stlplus.sourceforge.net/stlplus3/docs/file_system.html
    // added by chenyu    
    std::vector<std::string> vec_image = stlplus::folder_files( sImageDir );
    std::sort(vec_image.begin(), vec_image.end());

    // extract an array of filenames (not paths) of all the files found in the specified folder.
    // each of these names can be combined with folder and form _filespec() to give the 
    // filespec of each file.
    // More details: see http://stlplus.sourceforge.net/stlplus3/docs/file_system.html\
    // added by chenyu
    std::vector<std::string> vec_semantic_image = stlplus::folder_files( sSemanticImgDir);
    std::sort(vec_semantic_image.begin(), vec_semantic_image.end());

    // check if each image has a corresponding semantic segmentation image
    if(vec_image.size() != vec_semantic_image.size())
    {
            cerr << "images don't have a same size with semantic segmentation image" << endl;
            return EXIT_FAILURE;
    }
    for(int i = 0; i < vec_image.size(); i++)
    {
        if(stlplus::basename_part(vec_image[i]) != stlplus::basename_part(vec_semantic_image[i]))
        {
            cerr << vec_image[i] << "doesn't have a corresponding semantic segmentation image" << endl;
            return EXIT_FAILURE;
        }
    }

    // Configure an empty scene with Views and their corresponding cameras
    SfM_Data sfm_data;
    sfm_data.s_root_path = sImageDir; // Setup main image root_path
    sfm_data.s_seg_root_path = sSemanticImgDir; // Setup semantic image root_path
    Views & views = sfm_data.views;
    Intrinsics & intrinsics = sfm_data.intrinsics;
    std::vector<Vec3> gpsInfo;

    C_Progress_display my_progress_bar( vec_image.size(),
                                        std::cout, "\n- Image listing -\n" );
    std::ostringstream error_report_stream;
    int k = 0;
    for ( std::vector<std::string>::const_iterator iter_image = vec_image.begin();
          iter_image != vec_image.end();
          ++iter_image, ++my_progress_bar, ++k )
    {
        // Read meta data to fill camera parameter (w,h,focal,ppx,ppy) fields.
        width = height = ppx = ppy = focal = -1.0;

        const std::string sImageFilename = stlplus::create_filespec( sImageDir, *iter_image );

        // Test if the image format is supported:
        if (i23dSFM::image::GetFormat(sImageFilename.c_str()) == i23dSFM::image::Unknown)
        {
            error_report_stream
                    << stlplus::filename_part(sImageFilename) << ": Unkown image file format." << "\n";
            continue; // image cannot be opened
        }

        ImageHeader imgHeader;
        if (!i23dSFM::image::ReadImageHeader(sImageFilename.c_str(), &imgHeader))
            continue; // image cannot be read

        width = imgHeader.width;
        height = imgHeader.height;
        ppx = width / 2.0;
        ppy = height / 2.0;

        std::unique_ptr<Exif_IO> exifReader(new Exif_IO_EasyExif());
        exifReader->open( sImageFilename );

        const bool bHaveValidExifMetadata =
                exifReader->doesHaveExifInfo()
                && !exifReader->getBrand().empty()
                && !exifReader->getModel().empty();

        // Consider the case where the focal is provided manually
        if ( !bHaveValidExifMetadata || focal_pixels != -1)
        {
            if (sKmatrix.size() > 0) // Known user calibration K matrix
            {
                if (!checkIntrinsicStringValidity(sKmatrix, focal, ppx, ppy))
                    focal = -1.0;
            }
            else // User provided focal length value
            if (focal_pixels != -1 )
                focal = focal_pixels;
        }
        else // If image contains meta data
        {
            const std::string sCamName = exifReader->getBrand();
            const std::string sCamModel = exifReader->getModel();

            // Handle case where focal length is equal to 0
            if (exifReader->getFocal() == 0.0f)
            {
                error_report_stream
                        << stlplus::basename_part(sImageFilename) << ": Focal length is missing." << "\n";
                focal = -1.0;
            }
            else
                // Create the image entry in the list file
            {
                Datasheet datasheet;
                if ( getInfo( sCamName, sCamModel, vec_database, datasheet ))
                {
                    // The camera model was found in the database so we can compute it's approximated focal length
                    const double ccdw = datasheet._sensorSize;
                    focal = std::max ( width, height ) * exifReader->getFocal() / ccdw;
                }
                else
                {
                    error_report_stream
                            << stlplus::basename_part(sImageFilename) << ": Camera \""
                            << sCamName << "\" model \"" << sCamModel << "\" doesn't exist in the database" << "\n"
                            << "Please consider add your camera model and sensor width in the database." << "\n";
                }
            }
        }

        // Add gps info
        //std::cout << setprecision(6) << setiosflags(ios::fixed);
        //std::cout << exifReader->allExifData() << std::endl;
        Vec3 gpsPos;
        gpsPos[0] = exifReader->getLatitude();
        gpsPos[1] = exifReader->getLongitude();
        gpsPos[2] = exifReader->getAltitude();
        gpsPos[2] = (gpsPos[2] - LAND_HEIGHT) / focal * sqrt(width * width + height * height);
        gpsInfo.push_back(gpsPos);

        // Build intrinsic parameter related to the view
        std::shared_ptr<IntrinsicBase> intrinsic (NULL);
        if(focal < 0)
        {
            focal = std::max(width, height);
            error_report_stream << "Image " << sImageFilename << " focal length doesn't exist, set it to : " << focal << "\n";
        }

        if (focal > 0 && ppx > 0 && ppy > 0 && width > 0 && height > 0)
        {
            // Create the desired camera type
            switch(e_User_camera_model)
            {
                case PINHOLE_CAMERA:
                    intrinsic = std::make_shared<Pinhole_Intrinsic>
                            (width, height, focal, ppx, ppy);
                    break;
                case PINHOLE_CAMERA_RADIAL1:
                    intrinsic = std::make_shared<Pinhole_Intrinsic_Radial_K1>
                            (width, height, focal, ppx, ppy, 0.0); // setup no distortion as initial guess
                    break;
                case PINHOLE_CAMERA_RADIAL3:
                    intrinsic = std::make_shared<Pinhole_Intrinsic_Radial_K3>
                            (width, height, focal, ppx, ppy, 0.0, 0.0, 0.0);  // setup no distortion as initial guess
                    break;
                case PINHOLE_CAMERA_BROWN:
                    intrinsic =std::make_shared<Pinhole_Intrinsic_Brown_T2>
                            (width, height, focal, ppx, ppy, 0.0, 0.0, 0.0, 0.0, 0.0); // setup no distortion as initial guess
                    break;
                default:
                    std::cerr << "Error: unknown camera model: " << (int) e_User_camera_model << std::endl;
                    return EXIT_FAILURE;
            }
        }

        // Build the view corresponding to the image
        // View v(*iter_image, views.size(), views.size(), views.size(), width, height);
        // v.semantic_img_path = vec_semantic_image[k];
        View v(*iter_image, vec_semantic_image[k], views.size(), views.size(), views.size(), width, height);

        // Add intrinsic related to the image (if any)
        if (intrinsic == NULL)
        {
            //Since the view have invalid intrinsic data
            // (export the view, with an invalid intrinsic field value)
            v.id_intrinsic = UndefinedIndexT;
        }
        else
        {
            // Add the defined intrinsic to the sfm_container
            intrinsics[v.id_intrinsic] = intrinsic;
        }

        // Add the view to the sfm_container
        views[v.id_view] = std::make_shared<View>(v);
    }

    // Display saved warning & error messages if any.
    if (!error_report_stream.str().empty())
    {
        std::cerr
                << "\nWarning & Error messages:" << std::endl
                << error_report_stream.str() << std::endl;
    }

    // Generate  pair list according to the gps info
    if(bExifGpsInfo)
    {
        ofstream ofPairList(stlplus::create_filespec( sOutputDir, "pair_list.txt" ).c_str());
        int i, j;
        for(i = 0; i < gpsInfo.size(); i++)
        {
            // No GPS EXIF infomation
            if((fabs(gpsInfo[i][0]) < 1e-6) &&
               (fabs(gpsInfo[i][1]) < 1e-6) &&
               (fabs(gpsInfo[i][2]) < 1e-6))
            {
                ofPairList << i;
                for(j = 0; j < gpsInfo.size(); j++)
                    if(j != i)
                        ofPairList << " " << j;
                ofPairList << "\n";
            }
                // Find near photos
            else
            {
                bool hasNearPhoto = false;
                for(j = 0; j < gpsInfo.size(); j++)
                    if(i != j)
                    {
                        //cout << calcDistanceByGPS(gpsInfo[i], gpsInfo[j]) << " "
                        //     << gpsInfo[i][2] << " "
                        //     << gpsInfo[j][2] << endl;
                        if(calcDistanceByGPS(gpsInfo[i], gpsInfo[j]) < max(gpsInfo[i][2], gpsInfo[j][2]))
                        {
                            if(!hasNearPhoto)
                            {
                                ofPairList << i;
                                hasNearPhoto = true;
                            }
                            ofPairList << " " << j;
                        }
                    }
                if(hasNearPhoto)
                    ofPairList << "\n";
            }
        }
        ofPairList.close();
    }

    // Group camera that share common properties if desired (leads to more faster & stable BA).
    if (b_Group_camera_model)
    {
        GroupSharedIntrinsics(sfm_data);
    }

    // Store SfM_Data views & intrinsic data
    if (!Save(
            sfm_data,
            stlplus::create_filespec( sOutputDir, "sfm_data.json" ).c_str(),
            ESfM_Data(VIEWS|INTRINSICS)))
    {
        return EXIT_FAILURE;
    }

    std::cout << std::endl
              << "SfMInit_ImageListing report:\n"
              << "listed #File(s): " << vec_image.size() << "\n"
              << "usable #File(s) listed in sfm_data: " << sfm_data.GetViews().size() << std::endl;

    return EXIT_SUCCESS;
}
