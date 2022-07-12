import os

from utility import depth_io
from utility import image_io

from utility.logger import Logger
log = Logger(__name__)
log.logger.propagate = False

import replica_render

"""
1) cubemap data convert to pano data;
2) 
"""

# def pano_depth2pointcloud(dataroot_dir, frame_number, pano_depthmap_filename_exp, pano_pointcloud_filename_exp):
#     """Project the depth map to point clouds.
#     """
#     for image_index in range(0, frame_number):
#         if image_index % 10 == 0:
#             print(f"{frame_number} : {image_index}")
#         erp_depth_filepath = dataroot_dir + pano_depthmap_filename_exp.format(image_index)
#         erp_pointcloud_filepath = dataroot_dir + pano_pointcloud_filename_exp.format(image_index)
#         depth_data = depth_io.read_dpt(erp_depth_filepath)
#         # image_io.image_show(depth_data)
#         pointcloud_utils.depthmap2pointcloud_erp(depth_data, None, erp_pointcloud_filepath)


def flow_warp_cubemap_folder(dataroot_dir, frame_number, cubemap_rgb_image_filename_exp, cubemap_opticalflow_filename_exp,  cubemap_opticalflow_warp_filename_exp):
    """Warp the face image with face flow.
    """
    for image_index in range(0, frame_number):
        if image_index % 10 == 0:
            print(f"{frame_number} : {image_index}")
        # 1) warp the cube map image with cube map flow
        for facename_abbr in replica_render.ReplicaDataset.replica_cubemap_face_abbr:
            image_path = dataroot_dir + cubemap_rgb_image_filename_exp.format(image_index, facename_abbr)
            image_data = image_io.image_read(image_path)
            flow_path = dataroot_dir + cubemap_opticalflow_filename_exp.format(image_index, facename_abbr)
            flow_data = flow_io.flow_read(flow_path)

            face_warp_image = flow_warp.warp_forward(image_data, flow_data)
            cubemap_flow_warp_name = dataroot_dir + cubemap_opticalflow_warp_filename_exp.format(image_index, facename_abbr)
            image_io.image_save(face_warp_image, cubemap_flow_warp_name)
            # image_io.image_show(face_flow_vis)


def flow_warp_pano_folder(dataroot_dir, output_dir, frame_number, pano_rgb_image_filename_exp, pano_opticalflow_filename_exp,  pano_opticalflow_warp_filename_exp):
    """Warp the face image with face flow.
    """
    for image_index in range(0, frame_number):
        if image_index % 10 == 0:
            print(f"{frame_number} : {image_index}")
        # 1) warp the cube map image with cube map flow
        image_path = dataroot_dir + pano_rgb_image_filename_exp.format(image_index)
        image_data = image_io.image_read(image_path)
        flow_path = dataroot_dir + pano_opticalflow_filename_exp.format(image_index)
        flow_data = flow_io.flow_read(flow_path)

        face_warp_image = flow_warp.warp_forward(image_data, flow_data, True)
        cubemap_flow_warp_name = output_dir + pano_opticalflow_warp_filename_exp.format(image_index)
        image_io.image_save(face_warp_image, cubemap_flow_warp_name)
        # image_io.image_show(face_flow_vis)


def generate_depth_mask_folder(data_dir, output_dir, frame_number, pano_depthmap_filename_exp, pano_mask_filename_exp):
    """ Generate image mask from depth map.
    """
    for image_index in range(0, frame_number):
        if image_index % 10 == 0:
            log.info("Image index: {}".format(image_index))
        image_path = data_dir + pano_depthmap_filename_exp.format(image_index)
        depthdata = depth_io.read_dpt(image_path)
        mask_filepath = output_dir + pano_mask_filename_exp.format(image_index)
        depth_io.create_depth_mask(depthdata, mask_filepath, 0.0)


def cube2pano_rgb(data_dir, output_dir, frame_number, cubemap_rgb_image_filename_exp, pano_rgb_image_filename_exp):
    """ Convert the cubemap images to ERP image.
    """
    for image_index in range(0, frame_number):
        if image_index % 10 == 0:
            log.info("Image index: {}".format(image_index))
        # 1) load the 6 image to memory.
        face_images_src = []
        for facename_abbr in replica_render.ReplicaDataset.replica_cubemap_face_abbr:
            image_path = data_dir + cubemap_rgb_image_filename_exp.format(image_index, facename_abbr)
            face_images_src.append(image_io.image_read(image_path))

        # 2) test stitch the cubemap images
        erp_image_data = proj_cm.cubemap2erp_image(face_images_src, 0.0)
        # image_io.image_show(erp_image_src)
        erp_image_filepath = output_dir + pano_rgb_image_filename_exp.format(image_index)
        image_io.image_save(erp_image_data, erp_image_filepath)


def cube2pano_depthmap(data_dir, output_dir, frame_number, cubemap_depthmap_filename_exp, pano_depthmap_filename_exp, pano_depthmap_visual_filename_exp):
    """ Convert the cubemap depth map to ERP image.
    """
    for image_index in range(0, frame_number):
        if image_index % 10 == 0:
            log.info("Image index: {}".format(image_index))
        # 1) load the 6 image to memory.
        face_depth_list = []
        for facename_abbr in replica_render.ReplicaDataset.replica_cubemap_face_abbr:
            image_path = data_dir + cubemap_depthmap_filename_exp.format(image_index, facename_abbr)
            print(image_path)
            face_depth_list.append(depth_io.read_dpt(image_path))

        # 2) stitch the cubemap images
        erp_depth_data = proj_cm.cubemap2erp_depth(face_depth_list, padding_size=0.0)
        erp_depth_filepath = output_dir + pano_depthmap_filename_exp.format(image_index)
        # image_io.image_show(erp_depth_data)
        depth_io.write_dpt(erp_depth_data, erp_depth_filepath)
        # erp_depth_visual_filepath = output_dir + pano_depthmap_visual_filename_exp.format(image_index)
        # depth_io.depth_visual_save(erp_depth_data, erp_depth_visual_filepath)


def cube2pano_opticalflow(data_dir, output_dir, frame_number, cubemap_opticalflow_filename_exp, pano_opticalflow_filename_exp, pano_opticalflow_visual_filename_exp):
    """ Convert the cubemap images to ERP image.
    """
    for image_index in range(0, frame_number):
        if image_index % 10 == 0:
            log.info("Image index: {}".format(image_index))
        # 1) load the 6 image to memory.
        face_flo_list = []
        for facename_abbr in replica_render.ReplicaDataset.replica_cubemap_face_abbr:
            image_path = data_dir + cubemap_opticalflow_filename_exp.format(image_index, facename_abbr)
            print(image_path)
            face_flo_list.append(flow_io.read_flow_flo(image_path))

        # 2) stitch the cubemap images
        erp_of_data = proj_cm.cubemap2erp_flow(face_flo_list, padding_size=0.0, wrap_around=True)
        erp_of_filepath = output_dir + pano_opticalflow_filename_exp.format(image_index)
        flow_io.flow_write(erp_of_data, erp_of_filepath)
        # erp_of_vis = flow_vis.flow_to_color(erp_of_data, min_ratio=0.1, max_ratio=0.9)
        # erp_of_visual_filepath = output_dir + pano_opticalflow_visual_filename_exp.format(image_index)

        # TODO  one pixel at left side is wrong, larger the image size, how to deal with the point in the boundary
        # new_u = np.where(erp_of_data[:,:,0] > 2000, erp_of_data[:,:,0], 2000)
        # from skimage.morphology import dilation, square
        # # image_io.image_show(dilation(new_u, square(15)))
        # image_io.image_show(new_u)
        # flow_vis.flow_value_to_color(erp_of_data, erp_of_visual_filepath + "_0.jpg")

        # image_io.image_save(erp_of_vis, erp_of_visual_filepath)


def visual_data_folder(data_dir):
    """ Visualize the render result.

    :param data_dir: The path of data folder.
    :type data_dir: str
    """
    counter = 0
    for filename in os.listdir(data_dir):
        counter = counter + 1
        if counter % 10 == 0:
            print(f"{counter} : {filename}")
        if filename.endswith(".dpt"):
            depth_data = depth_io.read_dpt(data_dir + filename)
            # depth_io.depth_visual_save(depth_data, data_dir + filename + ".jpg", 0, 1.0)
            # # create the mask
            # depth_io.create_depth_mask(depth_data, data_dir + filename + "_mask.png", 0.0)
        elif filename.endswith(".flo"):  # and filename == "0002_R_motionvector_forward.flo":
            of_data = flow_io.read_flow_flo(data_dir + filename)
            # of_data_vis = flow_vis.flow_to_color(of_data)#,  min_ratio=0.3, max_ratio=0.97)
            # image_io.image_save(of_data_vis, data_dir + filename + ".jpg")
            print("visual optical flow {}".format(filename))
            flow_vis.flow_value_to_color(of_data, min_ratio=0.2, max_ratio=0.8)
            # of_data_vis_uv = flow_vis.flow_max_min_visual(of_data, None)#"D:/1.jpg")