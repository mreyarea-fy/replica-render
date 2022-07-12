import math
import os.path
from random import randint
import numpy as np
import pathlib

import replica_render
from utility.logger import Logger
log = Logger(__name__)
log.logger.propagate = False
"""
Generate render camera path from
"""

def json_parse(dict_data):
    """
    Load the dict from JSON file.
    """
    def _cam_param_json2dict(dict_data):
        for key, value in dict_data.items():
            if isinstance(value, list):
                dict_data[key] = np.asarray(value)
            elif isinstance(value, dict):
                dict_data[key] = _cam_param_json2dict(value)
        return dict_data

    # parer dict and translate the list to numpy array
    _cam_param_json2dict(dict_data)
    return dict_data


def create_camera_traj_obj(center, camera_position_list, obj_file_path):
    """
    output the camera and center position to *.obj 
    and generate a 
    """
    # add center point, index is 1
    vec_str_list = "v {} {} {}\n".format(center[0], center[1], center[2])
    face_str_list = ""
    for idx, camera_position in enumerate(camera_position_list):
        # add vertex
        vec_str = "v {} {} {}\n".format(camera_position[1], camera_position[2], camera_position[3])
        vec_str_list = vec_str_list + vec_str
        # add face
        if idx > 0:
            face_str = "f 1 {} {}\n".format(idx + 1, idx + 2)
            face_str_list = face_str_list + face_str

    # face for last vertex
    face_str = "f 1 {} 2\n".format(len(camera_position_list) + 1)
    face_str_list = face_str_list + face_str

    obj_str = vec_str_list + face_str_list
    with open(obj_file_path, "wt") as obj_file:
        obj_file.write(obj_str)


def generate_path_grid_circle(scene_name, grid_size, radius, path_csv_file, center_csv_file,
                              center_point, lock_direction_enable=True):
    """
     the unit is Meter
     the circle in the plane of x_y
    """
    cx = center_point[0]
    cy = center_point[1]
    cz = center_point[2]

    radius_step = int(float(radius) / grid_size + 0.5) + 1
    x_min = cx - radius_step * grid_size
    x_max = cx + radius_step * grid_size
    y_min = cy - radius_step * grid_size
    y_max = cy + radius_step * grid_size

    navigable_positions = []

    counter = 0
    for x in np.arange(x_min, x_max + grid_size * 0.1, grid_size):
        for y in np.arange(y_min, y_max + grid_size * 0.1, grid_size):
            # skip the position out the circle
            if math.sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) - np.finfo(float).eps * 10 > radius:
                continue

            spot = [counter]
            counter = counter + 1

            # camera position
            position_x = x
            position_y = y
            position_z = cz
            spot += [position_x, position_y, position_z]

            # camera orientation (outward)
            if lock_direction_enable:
                raise ValueError("do not implement lock direction")
            else:
                spot += [0.0, 0.0, 0.0]

            navigable_positions.append(spot)

    # output camera pose file for render
    with open(path_csv_file, 'w') as f:
        f.writelines(' '.join(str(j) for j in i) + '\n' for i in navigable_positions)
    print("output path file {}".format(path_csv_file))

    # output the camera centre csv file
    with open(center_csv_file, 'w') as f:
        f.writelines("0 {} {} {} {} {} {} \n".format(cx, cy, cz, 0.0, 0.0, 0.0))
    print("output centre file {}".format(center_csv_file))

    # output camera position obj file
    path_obj_file = path_csv_file + ".obj"
    create_camera_traj_obj([cx, cy, cz + 0.07], navigable_positions, path_obj_file)
    print("output path 3D model file {}".format(path_obj_file))

    return counter


def generate_path_circle(scene_name, steps, radius, path_csv_file, center_csv_file,
                    initial_rotation, center_point=None, lock_direction_enable=False):
    '''
    Args:
        scene_name: 
        steps: the number of frame
        path_csv_file: the csv file store the postion and orientation
        center_csv_file: store the center of camera
        center_point: list of xyz
    '''
    if center_point is None:
        # open position file
        #samples a random valid starting position
        scene_centre_samples_file = "../glob/" + scene_name + ".txt"
        print("select center point from file {}".format(scene_centre_samples_file))
        scenePos = open(scene_centre_samples_file, "r")
        data = [[float(i) for i in line.split()] for line in scenePos]

        idx = randint(0, len(data))
        idx = 221
        cx = data[idx][0]
        cy = data[idx][1]
        cz = data[idx][2] + 0.5
    else:
        cx = center_point[0]
        cy = center_point[1]
        cz = center_point[2]

    print("Sampled center position:", cx, cy, cz)
    # output the camera centre csv file
    with open(center_csv_file, 'w') as f:
        f.writelines("0 {} {} {} {} {} {} \n".format(cx, cy, cz, 0.0, 0.0, 0.0))
    print("output centre file {}".format(center_csv_file))

    # generate and output the camera path csv file
    navigable_positions = []
    for i in range(steps):
        spot = [i]

        rad = float(i) / steps * math.pi * 2

        # camera position
        position_x = cx + math.cos(rad)*radius
        position_y = cy + math.sin(rad)*radius
        position_z = cz
        spot += [position_x, position_y, position_z]

        # camera orientation (outward)
        if lock_direction_enable:
            camera_rotation_x = initial_rotation[0]
            camera_rotation_y = initial_rotation[1]
            camera_rotation_z =  np.degrees(rad)# rad / math.pi * 180
            spot += [camera_rotation_x, camera_rotation_y, camera_rotation_z]
        else:
            spot += [0, 0, 0]

        # append the data
        navigable_positions.append(spot)

    # output camera pose file for render
    with open(path_csv_file, 'w') as f:
        f.writelines(' '.join(str(j) for j in i) + '\n' for i in navigable_positions)
    print("output path file {}".format(path_csv_file))

    # output camera position obj file
    path_obj_file = path_csv_file + ".obj"
    create_camera_traj_obj([cx, cy, cz], navigable_positions, path_obj_file)
    print("output path 3D model file {}".format(path_obj_file))


def generate_path_line(scene_name, line_segment_size, line_length, path_csv_file, center_csv_file,
                              center_point, center_point_rotation):
    """
    The line is parallel to x-axis.
    """
    cx = center_point[0]
    cy = center_point[1]
    cz = center_point[2]

    segment_length = float(line_length) / line_segment_size 
    x_start = cx - line_length / 2.0
    navigable_positions = []
    counter = 0
    for segments_index in range(0, line_segment_size):
        # skip the position out the circle
        spot = [counter]
        counter = counter + 1
        # camera position
        position_x = x_start + segment_length * segments_index
        position_y = cy 
        position_z = cz
        spot += [position_x, position_y, position_z]

        # camera orientation (outward)
        spot += center_point_rotation

        navigable_positions.append(spot)

    # output camera pose file for render
    with open(path_csv_file, 'w') as f:
        f.writelines(' '.join(str(j) for j in i) + '\n' for i in navigable_positions)
    print("output path file {}".format(path_csv_file))

    # output the camera centre csv file
    with open(center_csv_file, 'w') as f:
        f.writelines("0 {} {} {} {} {} {} \n".format(cx, cy, cz, 0.0, 0.0, 0.0))
    print("output centre file {}".format(center_csv_file))

    # output camera position obj file
    path_obj_file = path_csv_file + ".obj"
    create_camera_traj_obj([cx, cy, cz + 0.07], navigable_positions, path_obj_file)
    print("output path 3D model file {}".format(path_obj_file))

    return counter


def generate_path_random(scene_name, camera_pose_number, camera_position_range, camera_rotation_range,
                                            path_csv_file, center_csv_file,
                                            center_point, center_point_rotation):
    """ Create random camera position sequence.


    :param bounding_box: [description]
    :type bounding_box: numpy
    :param center_point: camera original pose's position
    :type center_point: list
    :param center_point_rotation:  camera original pose's position, degree
    :type center_point_rotation: list
    :return: [description]
    :rtype: [type]
    """                                  
    # 1) create random camera pose
    cx = center_point[0]
    cy = center_point[1]
    cz = center_point[2]

    camera_position_list = np.zeros((camera_pose_number, 3), dtype=np.float64)
    camera_orientation_list = np.zeros((camera_pose_number, 3), dtype=np.float64)

    # position_stddev = (camera_position_range[axis_idx][1] - camera_position_range[axis_idx][0]) / 2.0
    # camera_position_list[:, axis_idx] = np.random.normal(0, position_stddev, camera_pose_number)  \
    #     + camera_position_range[axis_idx][0] + center_point[axis_idx]

    # # rotation
    # rotation_stddev = (camera_rotation_range[axis_idx][1] - camera_rotation_range[axis_idx][0]) / 2.0
    # camera_orientation_list[:, axis_idx] = np.random.normal(0, rotation_stddev, camera_pose_number) \
    #     + camera_rotation_range[axis_idx][0] + center_point_rotation[axis_idx]

    counter = 0
    position_pre = np.array([cx, cy, cz], np.float64)
    while counter < camera_pose_number:
        position_cur = np.zeros((3), np.float64)
        orientation_cur = np.zeros((3), np.float64)
        for axis_idx in range(0, 3):
            # position
            position_cur[axis_idx] = np.random.uniform(camera_position_range[axis_idx][0], camera_position_range[axis_idx][1], 1) + center_point[axis_idx]

            # rotation
            orientation_cur[axis_idx] = np.random.uniform(camera_rotation_range[axis_idx][0], camera_rotation_range[axis_idx][1], 1) + center_point_rotation[axis_idx]

        # limit the two camera position distance in case optical flow stitch error
        dist = np.linalg.norm(position_cur-position_pre)
        points_distance_threshold = 0.3 
        if dist > points_distance_threshold:
            log.debug("Distance is {}, skip.".format(dist))
            continue

        position_pre = position_cur
        camera_position_list[counter, :] = position_cur
        camera_orientation_list[counter, :] = orientation_cur
        counter = counter + 1

    # 2) output file
    # output camera pose file for render
    with open(path_csv_file, 'w') as f:
        # f.writelines(' '.join(str(j) for j in i) + '\n' for i in navigable_positions)
        for camera_idx in range(camera_pose_number):
            cam_p_x, cam_p_y, cam_p_z = camera_position_list[camera_idx, :]
            cam_r_x, cam_r_y, cam_r_z = camera_orientation_list[camera_idx, :]
            f.write(f"{camera_idx} {cam_p_x} {cam_p_y} {cam_p_z} {cam_r_x} {cam_r_y} {cam_r_z}\n")
    print("output path file {}".format(path_csv_file))

    # output the camera centre csv file
    with open(center_csv_file, 'w') as f:
        f.writelines("0 {} {} {} {} {} {} \n".format(cx, cy, cz, 0.0, 0.0, 0.0))
    print("output centre file {}".format(center_csv_file))

    # output camera position obj file
    path_obj_file = path_csv_file + ".obj"
    camera_position_list_ = np.ones((camera_position_list.shape[0] + 8, camera_position_list.shape[1] + 1), dtype= np.float64)
    camera_position_list_[8:, 1:] = camera_position_list
    # add the bbox
    counter = 0
    for x_offset in camera_position_range[0]:
        for y_offset in camera_position_range[1]:
            for z_offset in camera_position_range[2]:
                camera_position_list_[counter, :] = [1, cx+x_offset, cy + y_offset, cz + z_offset]
                counter = counter + 1
    create_camera_traj_obj([cx, cy, cz + 0.07], camera_position_list_.tolist(), path_obj_file)
    print("output path camera position 3D model file {}".format(path_obj_file))

    return camera_pose_number


def generate_path(root_dir, config):
    """
    @return: path_csv_file, center_csv_file

    :param config: the rendering configuration load from *.json file.
    :type config: dict
    """
    # root_dir = pathlib.Path(root_dir_)
    scene_name = config["scene_name"]


    center_point_position = [
        config["camera_traj"]["center_position"]["x"],
        config["camera_traj"]["center_position"]["y"],
        config["camera_traj"]["center_position"]["z"]]
    center_point_rotation = [
        config["camera_traj"]["start_orientations"]["x"],
        config["camera_traj"]["start_orientations"]["y"],
        config["camera_traj"]["start_orientations"]["z"]]

    lock_direction_enable = config["camera_traj"]["lock_direction"]
    path_type = config["camera_traj"]["type"]

    log.info("center position is {}, rotation is {}.".format(center_point_position, center_point_rotation))

    output_filename = replica_render.ReplicaRenderConfig.render_camera_path_filename;
    output_center_filename = replica_render.ReplicaRenderConfig.render_camera_path_center_filename; 
    path_csv_file = os.path.join(root_dir, output_filename)
    pathlib.Path.mkdir(pathlib.Path(root_dir), exist_ok=True, parents=True)
    center_csv_file = os.path.join(root_dir, output_center_filename)

    frame_number = None
    if path_type == "circle":
        # # load center point, radius and step
        # input_center_filename = root_dir + "/circle_center.csv"
        # with open(input_center_filename, newline='') as csvfile:
        #     data = csv.reader(csvfile, delimiter=' ', quotechar='|')
        #     #next(data, None)  # skip the headers
        #     for row in data:
        #         if len(row) ==  0 or row[0] == '#':
        #             continue
        #         scene_name = row[0]
        #         nsteps = int(row[1])
        #         radius = float(row[2])
        #         center_point_position = [float(row[3]), float(row[4]), float(row[5])]
        #         center_point_rotation = [float(row[6]), float(row[7]), float(row[8])]
        #print("load camera infor from {}".format(input_center_filename))
        # generate path file
        # output_filename = str(nsteps) + "_" + str(radius) + "_circle.csv"
        # output_center_filename = str(nsteps) + "_" + str(radius) + "_circle_center.csv"
        # output_filename = "circle.csv"
        # output_center_filename = "circle_center.csv"

        # path_csv_file = root_dir + "/"+ output_filename
        # center_csv_file = root_dir + "/" + output_center_filename

        radius = config["camera_traj"]["radius"]
        nsteps = config["camera_traj"]["circle_step_number"]
        print("generate camera path for {}, view number is {}, radius is {}.".format(scene_name, nsteps, radius))

        generate_path_circle(scene_name, nsteps, radius,
                        path_csv_file, center_csv_file, center_point=center_point_position,
                        initial_rotation=center_point_rotation,
                        lock_direction_enable=lock_direction_enable)

        frame_number = nsteps

    elif path_type == "grid":
        # output_filename = "grid.csv"
        # output_center_filename = "grid_center.csv"

        # path_csv_file = root_dir + "/" + output_filename
        # center_csv_file = root_dir + "/" + output_center_filename

        radius = config["camera_traj"]["radius"]
        grid_size = config["camera_traj"]["grid_size"]
        frame_number = generate_path_grid_circle(scene_name, grid_size, radius, path_csv_file, center_csv_file,
                                  center_point=center_point_position, lock_direction_enable=lock_direction_enable)

    elif path_type == "line":
        # output_filename = "line.csv"
        # output_center_filename = "line_center.csv"

        # path_csv_file = root_dir + "/" + output_filename
        # center_csv_file = root_dir + "/" + output_center_filename
        radius = config["camera_traj"]["radius"]
        grid_size = config["camera_traj"]["grid_size"]
        frame_number =  generate_path_line(scene_name, grid_size, radius, path_csv_file, center_csv_file,
                                  center_point=center_point_position, center_point_rotation = center_point_rotation)

    elif path_type == "random":
        # output_filename = "random.csv"
        # output_center_filename = "random_center.csv"
        # path_csv_file = root_dir + "/" + output_filename
        # center_csv_file = root_dir + "/" + output_center_filename
        
        config_data = json_parse(config)
        camera_pose_number = config["camera_traj"]["grid_size"]
        camera_position_range = config_data["camera_traj"]["random_position_offset_range"]  # meter
        camera_rotation_range = config_data["camera_traj"]["random_rotation_offset_range"]  # degree
        frame_number = generate_path_random(scene_name, camera_pose_number, camera_position_range, camera_rotation_range,
                                            path_csv_file, center_csv_file,
                                            center_point=center_point_position, center_point_rotation=center_point_rotation)
    else:
        raise RuntimeError("Do not support {} camera path".format(path_type))

    return path_csv_file, center_csv_file, frame_number


