import os
import shutil
import subprocess
import json
import os
import subprocess
import platform
import csv
import argparse
import math

from utility import fs_utility
from utility import depth_io
from utility import image_io
from utility.logger import Logger
log = Logger(__name__)
log.logger.propagate = False

import replica_camera_pose
import replica_post_process

"""
Render panoramic 2D images, depth maps and optical flow.
"""

class ReplicaDataset():
    """The Replica raw data information and convention.
    1) dataset 3D data information;
    2) dataset 2D image information;
    3) intermedia data information;
    """

    """ 3D data convention. """
    # 0) the Replica-Dataset root folder
    if platform.system() == "Windows":
        replica_data_root_dir = "D:/dataset/replica_v1_0/"
    elif platform.system() == "Linux":
        replica_data_root_dir = "/home/manuel/data/Replica"

    # all dataset list
    replica_scene_name_list = [
        "apartment_0",
        "apartment_1",
        "apartment_2",
        "frl_apartment_0",
        "frl_apartment_1",
        "frl_apartment_2",
        "frl_apartment_3",
        "frl_apartment_4",
        "frl_apartment_5",
        "hotel_0",
        "office_0",
        "office_1",
        "office_2",
        "office_3",
        "office_4",
        "room_0",
        "room_1",
        "room_2"
    ]

    # original dataset model and texture
    replica_mesh_file = "mesh.ply"
    replica_texture_file = "textures"
    replica_glass_file = "glass.sur"

    """ 2D data convention.
    Image data file name for each scene data. """
    # cubemap filename expression
    
    replica_cubemap_face_abbr = ["R", "L", "U", "D", "F", "B"] #  +x, -x, +y, -y, +z, -z
    # {:04d} is the frame index
    replica_cubemap_rgb_image_filename_exp = "{:04d}_{}_rgb.jpg"
    replica_cubemap_depthmap_filename_exp = "{:04d}_{}_depth.dpt"
    replica_cubemap_opticalflow_forward_filename_exp = "{:04d}_{}_motionvector_forward.flo"
    replica_cubemap_opticalflow_backward_filename_exp = "{:04d}_{}_motionvector_backward.flo"

    replica_cubemap_rgb_froward_of_forwardwarp_filename_exp = "{:04d}_{}_motionvector_forward_rgb_forwardwarp.jpg"
    replica_cubemap_rgb_backward_of_forwardwarp_filename_exp = "{:04d}_{}_motionvector_backward_rgb_forwardwarp.jpg"

    # panoramic filename expresion
    replica_pano_rgb_image_filename_exp = "{:04d}_rgb_pano.png"

    replica_pano_depthmap_filename_exp = "{:04d}_depth_pano.dpt"
    replica_pano_depthmap_visual_filename_exp = "{:04d}_depth_pano_visual.jpg"
    replica_pano_depthmap_mask_filename_exp = "{:04d}_mask_pano.png"

    replica_pano_opticalflow_forward_filename_exp = "{:04d}_opticalflow_forward_pano.flo"
    replica_pano_opticalflow_forward_visual_filename_exp = "{:04d}_opticalflow_forward_pano_visual.jpg"
    replica_pano_opticalflow_backward_filename_exp = "{:04d}_opticalflow_backward_pano.flo"
    replica_pano_opticalflow_backward_visual_filename_exp = "{:04d}_opticalflow_backward_pano_visual.jpg"

    replica_pano_mask_filename_exp = "{:04d}_mask_pano.png"

    # warp result
    replica_pano_rgb_froward_of_forwardwarp_filename_exp = "{:04d}_opticalflow_forward_rgb_forwardwarp.jpg"
    replica_pano_rgb_backward_of_forwardwarp_filename_exp = "{:04d}_opticalflow_backward_rgb_forwardwarp.jpg"


class ReplicaRenderConfig():
    """Replica dataset rendering configuration.
    1) Replica Render programs location;
    2) Replica Render config files, to generate the render openGL camera locations information;
    """
    # 0) input configuration folder 
    input_config_root_dir = "/home/manuel/data/Replica-render/"
    input_config_json_filename = "config.json"

    # 1) the output root folder
    output_root_dir = "/home/manuel/data/Replica_360/"
    output_cubemap_dir = "cubemap/"
    output_pano_dir = "pano/"

    # 2) rendering programs path
    if platform.system() == "Windows":
        program_root_dir = "D:/workspace_windows/360_opticalflow/Rendering360OpticalFlow-replica/build_msvc/ReplicaSDK/Release/"
        render_panorama_program_filepath = program_root_dir + "ReplicaRendererPanorama.exe"
        render_cubemap_program_filepath = program_root_dir + "ReplicaRendererCubemap.exe"
    elif platform.system() == "Linux":
        program_root_dir = "/home/manuel/code/replica-render/build/ReplicaSDK/"
        render_panorama_program_filepath = os.path.join(program_root_dir, "ReplicaRendererPanorama")
        render_cubemap_program_filepath = os.path.join(program_root_dir, "ReplicaRendererCubemap")

    # 3-0) rendering configuration for whole dataset
    renderRGBEnable = True
    renderDepthEnable = True
    renderMotionVectorEnable = False
    renderUnavailableMask = False

    # camera render path
    render_camera_path_filename = "camera_traj.csv"
    render_camera_path_center_filename = "camera_traj_center.csv"

    # post process
    post_process_visualization = True

    # 3-1) the render configuration for each scene/config file
    def __init__(self):
        # the render and stitch configuration for each configuration file
        self.input_config_folder_list = [] # each folder one config file

        # the render configuration load from *.json files, the key is folder name
        self.render_scene_configs = {} 
        # render camera trajectory files
        self.render_scene_pose_files = {}
        self.render_scene_frame_number = {}

    def add_scene_sub_folder(self, folder_name, opt):
        """ Add sub folder of root dir will render.

        :param folder_name: The sub folder's name
        :type folder_name: str
        """
        # check exist config *.json
        input_config_path = os.path.join(self.input_config_root_dir, folder_name, self.input_config_json_filename)
        if not os.path.exists(input_config_path):
            log.warn("{} don't have {}, skip".format(folder_name, self.input_config_json_filename))
            return

        # load *.json camera path generation configuration
        with open(input_config_path) as json_file:
            config = None
            try:
                config = json.load(json_file)
                if opt:
                    config["render_type"] = opt.render_type
                    config["image"]["width"] = opt.width
                    config["image"]["height"] = opt.height
            except Exception as e:
                log.error("parse {} fail. message {}".format(input_config_path, e.message))
            self.render_scene_configs[folder_name] = config

        self.input_config_folder_list.append(folder_name)

        # log.info("The folder name postfix is : {}".format(folder_postfix))
        # for idx in range(len(super().replica_scene_name_list)):
        #     folder_name = super().replica_scene_name_list[idx] + folder_postfix
        #     # log.info("Render to folder {}".format(folder_name))
        #     self.input_config_folder_list.append(folder_name)


def generate_camera_traj_file(render_configs, overwrite=True):
    """Generate render openGL camera pose for each scene.

    :param render_configs: [description]
    :type render_configs: [type]
    :param overwrite: Overwirte exist camera pose file, defaults to True
    :type overwrite: bool, optional
    """
    # 0) genreate the csv camera pose file
    for render_folder_name in render_configs.input_config_folder_list:
        # # load camera path generation configuration
        # output_scene_dir = render_configs.output_root_dir + render_folder_name + "/"
        # scene_render_config_filepath = output_scene_dir + render_configs.config_json_filename
        # with open(scene_render_config_filepath) as json_file:
        config = render_configs.render_scene_configs[render_folder_name]
        log.debug("genreate the camera pose csv file for {}, {}".format(render_folder_name, config["scene_name"]))

            

        # TODO override original camera traj file?
        # csv_file_list = fs_utility.list_files(str(output_root_dir / dir_item), ".csv")
        # if not csv_file_list:
        #     log.info("Do not have exist camera path csv file in folder {}".format(str(output_root_dir / dir_item)))
        #     path_csv_file, _ = gen_video_path_mp.generate_path(str(output_root_dir / dir_item), replica_scene_config)
        # else:
        #     csv_file_list.sort()
        #     path_csv_file = csv_file_list[0]
        #     path_csv_file = str(output_root_dir / dir_item) + "/" + path_csv_file
        #     log.info("use exited camera path csv file {} rendering.".format(path_csv_file))

        # genene camera path"
        output_scene_dir = os.path.join(render_configs.output_root_dir, render_folder_name)
        camera_pose_csv_filepath = os.path.join(output_scene_dir, ReplicaRenderConfig.render_camera_path_filename)
        render_configs.render_scene_pose_files[render_folder_name] = camera_pose_csv_filepath

        if os.path.exists(camera_pose_csv_filepath) and not overwrite:
            log.warn("exist camera traj file {}, skip.".format(camera_pose_csv_filepath))
            with open(camera_pose_csv_filepath) as file:
                frame_number = len(list(csv.reader(file)))
        else:
            camera_pose_csv_filepath, _, frame_number = replica_camera_pose.generate_path(output_scene_dir, config)
        render_configs.render_scene_frame_number[render_folder_name] = frame_number


def cubemap2pano(render_config, scene_folder_name, scene_frames_number):
    """ Convert the cubemap image/depth/optical flow. to 360 image/depth/optical flow.

    :param render_config: config for each scene.
    :type render_config: dict
    :param scene_folder_name: the scene folder's name.
    :type scene_folder_name: str
    """
    log.info("stitch cubemap to panoramic images for {}".format(scene_folder_name))
    pano_output_dir = ReplicaRenderConfig.output_root_dir + scene_folder_name + "/" + ReplicaRenderConfig.output_pano_dir
    replica_scene_data_root = ReplicaRenderConfig.output_root_dir + scene_folder_name + "/" + ReplicaRenderConfig.output_cubemap_dir

    fs_utility.dir_make(pano_output_dir)
    # stitch rgb image
    if ReplicaRenderConfig.renderRGBEnable:
        log.debug("stitch rgb image")
        replica_post_process.cube2pano_rgb(replica_scene_data_root,
                        pano_output_dir,
                        scene_frames_number,
                        ReplicaDataset.replica_cubemap_rgb_image_filename_exp,
                        ReplicaDataset.replica_pano_rgb_image_filename_exp)

    # stitch depth map
    if ReplicaRenderConfig.renderDepthEnable:
        log.debug("stitch depth map")
        replica_post_process.cube2pano_depthmap(replica_scene_data_root,
                            pano_output_dir,
                            scene_frames_number,
                            ReplicaDataset.replica_cubemap_depthmap_filename_exp,
                            ReplicaDataset.replica_pano_depthmap_filename_exp,
                            ReplicaDataset.replica_pano_depthmap_visual_filename_exp)
                            
    # stitch forward optical flow
    if ReplicaRenderConfig.renderMotionVectorEnable:
        log.debug("stitch forward optical flow")
        replica_post_process.cube2pano_opticalflow(replica_scene_data_root,
                                pano_output_dir,
                                scene_frames_number,
                                ReplicaDataset.replica_cubemap_opticalflow_forward_filename_exp,
                                ReplicaDataset.replica_pano_opticalflow_forward_filename_exp,
                                ReplicaDataset.replica_pano_opticalflow_forward_visual_filename_exp)

        # stitch backward optical flow
        log.debug("stitch backward optical flow")
        replica_post_process.cube2pano_opticalflow(replica_scene_data_root,
                                pano_output_dir,
                                scene_frames_number,
                                ReplicaDataset.replica_cubemap_opticalflow_backward_filename_exp,
                                ReplicaDataset.replica_pano_opticalflow_backward_filename_exp,
                                ReplicaDataset.replica_pano_opticalflow_backward_visual_filename_exp)


def render_pano(render_config, render_folder_name, camera_traj_file):
    """
    render dataset queue to panoramic 2D data.
    """
    # the render viewpoint
    render_view_center = render_config["render_view"]["center_view"]
    if render_view_center:
        log.warn("The pano-depthmap project do not need center view data.")
    render_view_traj = render_config["render_view"]["traj_view"]
    if not render_view_traj:
        log.error("Will not rendering data with camera path.")

    # create the replica rendering CLI parameters
    image_height = render_config["image"]["height"]
    image_width = render_config["image"]["width"]

    if image_width != image_height * 2:
        log.error(f"The image {image_width} is not the twice of {image_height}!")

    replica_data_root_dir = ReplicaDataset.replica_data_root_dir
    render_args_mesh = []
    render_args_mesh.append("--data_root")
    render_args_mesh.append(os.path.join(replica_data_root_dir, render_config["scene_name"]) + os.sep)
    render_args_mesh.append("--meshFile")
    render_args_mesh.append(ReplicaDataset.replica_mesh_file)
    render_args_mesh.append("--atlasFolder")
    render_args_mesh.append(ReplicaDataset.replica_texture_file)
    render_args_mesh.append("--mirrorFile")
    render_args_mesh.append(ReplicaDataset.replica_glass_file)

    render_args_imageinfo = []
    render_args_imageinfo.append("--imageHeight")
    render_args_imageinfo.append(str(image_height))

    render_args_texture_params = []
    render_args_texture_params.append("--texture_exposure")
    render_args_texture_params.append(str(render_config["render_params"]["texture_exposure"]))
    render_args_texture_params.append("--texture_gamma")
    render_args_texture_params.append(str(render_config["render_params"]["texture_gamma"]))
    render_args_texture_params.append("--texture_saturation")
    render_args_texture_params.append(str(render_config["render_params"]["texture_saturation"]))

    render_args_render_data = []
    render_args_render_data.append("--renderRGBEnable=" + str(ReplicaRenderConfig.renderRGBEnable))
    render_args_render_data.append("--renderDepthEnable=" + str(ReplicaRenderConfig.renderDepthEnable))
    render_args_render_data.append("--renderMotionVectorEnable=" + str(ReplicaRenderConfig.renderMotionVectorEnable))

    # 2camera viewpoint sequence
    render_args = [ReplicaRenderConfig.render_panorama_program_filepath]
    render_args = render_args + render_args_mesh +render_args_imageinfo + render_args_texture_params + render_args_render_data
    # add camera path file
    render_args.append("--cameraPoseFile")
    render_args.append(camera_traj_file)

    # render output folder
    render_scene_output_dir = os.path.join(ReplicaRenderConfig.output_root_dir, render_folder_name, ReplicaRenderConfig.output_pano_dir)
    render_args.append("--outputDir")
    print(render_scene_output_dir)
    render_args.append(render_scene_output_dir)
    fs_utility.dir_make(render_scene_output_dir)

    log.info(" ".join(render_args))

    try:
        render_seq_return = subprocess.run(render_args)
    except Exception as error:
        print(error)


def render_cubemap(render_config, render_folder_name, camera_traj_file):
    # check the configuration
    if render_config["render_view"]["center_view"]:
        log.warn("Do not render center viewpoint images.")

    # call the program to render cubemap
    render_args = []
    # if render_config["render_type"] == "cubemap":
    render_args.append(ReplicaRenderConfig.render_cubemap_program_filepath)
    render_args.append("--imageSize")
    render_args.append(str(render_config["image"]["height"]))
    render_scene_output_dir = ReplicaRenderConfig.output_root_dir + render_folder_name + "/" + ReplicaRenderConfig.output_cubemap_dir
    fs_utility.dir_make(render_scene_output_dir)

    render_args.append("--data_root")
    render_args.append(ReplicaDataset.replica_data_root_dir + render_config["scene_name"] + "/")
    render_args.append("--meshFile")
    render_args.append(ReplicaDataset.replica_mesh_file)
    render_args.append("--atlasFolder")
    render_args.append(ReplicaDataset.replica_texture_file)
    if not render_config["scene_name"] in ["frl_apartment_1"]:
        render_args.append("--mirrorFile")
        render_args.append(ReplicaDataset.replica_glass_file)
    render_args.append("--cameraPoseFile")
    render_args.append(camera_traj_file)
    render_args.append("--outputDir")
    render_args.append(render_scene_output_dir)
    render_args.append("--texture_exposure")
    render_args.append(str(render_config["render_params"]["texture_exposure"]))
    render_args.append("--texture_gamma")
    render_args.append(str(render_config["render_params"]["texture_gamma"]))
    render_args.append("--texture_saturation")
    render_args.append(str(render_config["render_params"]["texture_saturation"]))
    render_args.append("--renderRGBEnable=" + str(ReplicaRenderConfig.renderRGBEnable))
    render_args.append("--renderDepthEnable=" + str(ReplicaRenderConfig.renderDepthEnable))
    render_args.append("--renderMotionVectorEnable=" + str(ReplicaRenderConfig.renderMotionVectorEnable))

    # run the render program
    log.debug(render_args)
    render_seq_return = subprocess.check_call(render_args)


def post_render(render_configs, render_folder_name, scene_frame_number):
    """ Create depth map mask and visualize data.

    :param render_configs: The rendering configuration.
    :type render_configs: dict
    :param render_folder_name: The scene folder name.
    :type render_folder_name: str
    :param scene_frame_number: The scene rendered frame number.
    :type scene_frame_number: int
    """
    pano_output_dir = ReplicaRenderConfig.output_root_dir + render_folder_name + "/" + ReplicaRenderConfig.output_pano_dir
    if ReplicaRenderConfig.renderUnavailableMask and ReplicaRenderConfig.renderDepthEnable:
        # generate unavailable pixel mask
        log.info("generate depth map unavailable pixels mask image.")
        replica_post_process.generate_depth_mask_folder(pano_output_dir,
                        pano_output_dir,
                        scene_frame_number,
                        ReplicaDataset.replica_pano_depthmap_filename_exp,
                        ReplicaDataset.replica_pano_mask_filename_exp)
    

    # visualize depth map and optical flow
    if ReplicaRenderConfig.post_process_visualization:
        if ReplicaRenderConfig.renderDepthEnable:
            for image_index in range(0, scene_frame_number):
                if image_index % 10 == 0:
                    log.info("Image index: {}".format(image_index))

                erp_depth_filepath = pano_output_dir + ReplicaDataset.replica_pano_depthmap_filename_exp.format(image_index)
                erp_depth_visual_filepath = pano_output_dir + ReplicaDataset.replica_pano_depthmap_visual_filename_exp.format(image_index)
                erp_depth_data = depth_io.read_dpt(erp_depth_filepath)
                depth_io.depth_visual_save(erp_depth_data, erp_depth_visual_filepath)

        if ReplicaRenderConfig.renderMotionVectorEnable:
            for image_index in range(0, scene_frame_number):
                if image_index % 10 == 0:
                    log.info("Image index: {}".format(image_index))

                erp_of_filepath = pano_output_dir + ReplicaDataset.replica_pano_opticalflow_forward_filename_exp.format(image_index)
                erp_of_visual_filepath = pano_output_dir + ReplicaDataset.replica_pano_opticalflow_forward_visual_filename_exp.format(image_index)
                erp_of_data = flow_io.flow_read(erp_of_filepath)
                erp_of_vis = flow_vis.flow_to_color(erp_of_data, min_ratio=0.1, max_ratio=0.9)
                image_io.image_save(erp_of_vis, erp_of_visual_filepath)

                erp_of_filepath = pano_output_dir + ReplicaDataset.replica_pano_opticalflow_backward_filename_exp.format(image_index)
                erp_of_visual_filepath = pano_output_dir + ReplicaDataset.replica_pano_opticalflow_backward_visual_filename_exp.format(image_index)
                erp_of_data = flow_io.flow_read(erp_of_filepath)
                erp_of_vis = flow_vis.flow_to_color(erp_of_data, min_ratio=0.1, max_ratio=0.9)
                image_io.image_save(erp_of_vis, erp_of_visual_filepath)


def render_dataset_pano(render_configs):
    """ Render panoramic data.
    This function render cubemap and generate panoramic images by post-processing.

    :param render_configs: The render configuration
    :type render_configs: ReplicaRenderConfig
    """

    for config_folder in render_configs.input_config_folder_list:
        if not render_configs.render_scene_configs[config_folder]["scene_name"] in ReplicaDataset.replica_scene_name_list:
            log.error("folder scene {} is unavailable".format(config_folder))

    # 1) create camera path
    generate_camera_traj_file(render_configs, overwrite=True)

    # 2) render 2D data
    for render_subfolder_name in render_configs.input_config_folder_list:
        render_config = render_configs.render_scene_configs[render_subfolder_name]
        scene_name = render_config["scene_name"]
        if not scene_name in ReplicaDataset.replica_scene_name_list:
            log.warn("The scene {} do not in the Replica dataset scene list.".format(scene_name))
            continue

        camera_traj_file =  render_configs.render_scene_pose_files[render_subfolder_name]
        if render_config["render_type"] == "cubemap":
            log.info("render the cubemap data for {}".format(render_subfolder_name))
            render_cubemap(render_config, render_subfolder_name,  camera_traj_file)
            # stitch cubemap to panoramic images
            cubemap2pano(render_config, render_subfolder_name, render_configs.render_scene_frame_number[render_subfolder_name])
        elif render_config["render_type"] == "panorama":
            log.info("render the 360 data for {}".format(render_subfolder_name))
            render_pano(render_config, render_subfolder_name, camera_traj_file)
        else:
            log.error("Do not support render type: {}".format(render_config["render_type"]))

        # generate unavailable mask image & visualize depth map
        scene_frame_number = render_configs.render_scene_frame_number[render_subfolder_name]
        post_render(render_configs, render_subfolder_name, scene_frame_number)


def duplicate_replica_config(data_root_dir_pre, data_root_dir_new, replace_words_pairs = None, clean_copy = False):
    """ Duplicate the old replica rendering config and folder name to a new folder.
    1) build folder have same name as the previous;
    2) copy the previous *.json configuration file;
    
    :param data_root_dir_pre: the root folder of the previous output folder.
    :type data_root_dir_pre: str
    :param data_root_dir_new: the target folder path.
    :type data_root_dir_new: str
    :param replace_words_pairs: The words need be replaced. {previous: new}
    :type replace_words_pairs: dict
    """
    if clean_copy:
        fs_utility.dir_rm(data_root_dir_new)
        fs_utility.dir_make(data_root_dir_new)

    for dir_item in os.listdir(data_root_dir_pre):
        if not os.path.isdir(os.path.join(data_root_dir_pre, dir_item)):
            continue

        scene_output_root_dir_pre = data_root_dir_pre + dir_item  + "/"
        json_config_filelist = fs_utility.list_files(scene_output_root_dir_pre,".json")
        csv_config_filelist = [] #fs_utility.list_files(scene_output_root_dir_pre, ".csv")

        # check json file exist 
        if not json_config_filelist: # or not csv_config_filelist:
            continue

        # 1) make folder and copy file
        log.info("make folder and copy files from {}".format(scene_output_root_dir_pre))

        # 1-1) make folder
        scene_output_root_dir_new = data_root_dir_new + dir_item + "/"
        fs_utility.dir_make(scene_output_root_dir_new)

        # 1-2) copy file and replace word 
        for file_item in json_config_filelist + csv_config_filelist:
            src_filepath = data_root_dir_pre + dir_item + "/" + file_item
            tar_filepath = data_root_dir_new + dir_item + "/" + file_item
            log.info(f"copy file from {src_filepath} to {tar_filepath}")
            # shutil.copy(src_filepath, tar_filepath)
            fs_utility.copy_replace(src_filepath, tar_filepath, replace_words_pairs)


def collect_data(rendering_result_dir, release_result_dir):
    """ Copy the rendering result to new folder.
    Collect the final panoramic render result (RGB/Depth Map/Optical Flow) to target folder.

    :param rendering_result_dir: The render program output folder.s
    :type rendering_result_dir: str
    :param release_result_dir: The final panoramic data folder.
    :type release_result_dir: str
    """
    for folder_name in fs_utility.list_files(rendering_result_dir):
        # 0) it's available folder
        pano_render_result_dir = rendering_result_dir + folder_name + "/" + ReplicaRenderConfig.output_pano_dir
        if not os.path.exists(pano_render_result_dir):
            log.info("{} do not include panoramic data.".format(pano_render_result_dir))
            continue

        log.info("copy files form folder {}".format(folder_name))
        # 1) make new folder and copy file
        folder_name = folder_name[:-5]
        pano_release_result_dir = release_result_dir + folder_name + "/" 
        fs_utility.dir_make(pano_release_result_dir)

        counter = 0
        while(True):
            rgb_filename = ReplicaDataset.replica_pano_rgb_image_filename_exp.format(counter)
            if not os.path.exists(pano_render_result_dir + rgb_filename):
                break;
            shutil.copyfile(pano_render_result_dir + rgb_filename, pano_release_result_dir + rgb_filename)

            # depth map & pixel mask
            depthmap_filename = ReplicaDataset.replica_pano_depthmap_filename_exp.format(counter)
            if os.path.exists(pano_render_result_dir + depthmap_filename):
                shutil.copyfile(pano_render_result_dir + depthmap_filename, pano_release_result_dir + depthmap_filename)

            depthmap_mask_filename = ReplicaDataset.replica_pano_depthmap_mask_filename_exp.format(counter)
            if os.path.exists(pano_render_result_dir + depthmap_mask_filename):
                shutil.copyfile(pano_render_result_dir + depthmap_mask_filename, pano_release_result_dir + depthmap_mask_filename)

            # optical flow files
            of_forward_filename = ReplicaDataset.replica_pano_opticalflow_forward_filename_exp.format(counter)
            if os.path.exists(pano_render_result_dir + of_forward_filename):
                shutil.copyfile(pano_render_result_dir + of_forward_filename, pano_release_result_dir + of_forward_filename)

            of_backward_filename = ReplicaDataset.replica_pano_opticalflow_backward_filename_exp.format(counter)
            if os.path.exists(pano_render_result_dir + of_backward_filename):
                shutil.copyfile(pano_render_result_dir + of_backward_filename, pano_release_result_dir + of_backward_filename)

            counter += 1
            if counter % 10 == 0:
                log.info("copy file {}".format(rgb_filename))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--render_type", type=str, default="panorama", choices=["panorama", "cubemap"])
    parser.add_argument("--height", type=int, default=512)
    parser.add_argument("--width", type=int, default=1024)
    opt = parser.parse_args()

    if not math.log2(opt.width).is_integer():
        log.error(f"Width {opt.width} should be power of 2")

    suffix = None
    if opt.width == 256:
        suffix = "256"
    elif opt.width == 512:
        suffix = "512"
    elif opt.width == 1024:
        suffix = "1k"
    elif opt.width == 2048:
        suffix = "2k"
    elif opt.width == 4096:
        suffix = "4k"

    render_config = ReplicaRenderConfig()
    render_config.input_config_root_dir = "../config_files"
    render_config.output_root_dir = "/home/manuel/data/Replica_360/"
    ReplicaRenderConfig.output_pano_dir = ReplicaRenderConfig.output_pano_dir.split("/")[0] + f"_{suffix}/"
    config_folders = [os.path.basename(x[0]) for x in os.walk(render_config.input_config_root_dir)]
    for cf in config_folders:
        render_config.add_scene_sub_folder(cf, opt)

    generate_camera_traj_file(render_config, overwrite=False)

    render_dataset_pano(render_config)
