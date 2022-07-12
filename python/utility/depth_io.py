import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.cm as cm

import numpy as np

from struct import unpack
import os
import sys
import re

from . import fs_utility
from .logger import Logger

log = Logger(__name__)
log.logger.propagate = False


def depth_visual_save(depth_data, output_path, overwrite=True):
    """save the visualized depth map to image file with value-bar.

    :param dapthe_data: The depth data.
    :type dapthe_data: numpy
    :param output_path: the absolute path of output image.
    :type output_path: str
    """
    depth_data_temp = depth_data.astype(np.float64)

    if fs_utility.exist(output_path, 1) and overwrite:
        log.warn("{} exist.".format(output_path))
        fs_utility.file_rm(output_path)

    # draw image
    fig = plt.figure()
    plt.subplots_adjust(left=0, bottom=0, right=0.1, top=0.1, wspace=None, hspace=None)
    ax = fig.add_subplot(111)
    ax.get_xaxis().set_visible(False)
    ax.get_yaxis().set_visible(False)
    fig.tight_layout()
    im = ax.imshow(depth_data_temp, cmap="turbo")
    cbar = ax.figure.colorbar(im, ax=ax)
    plt.savefig(output_path, dpi=150)
    # plt.imsave(output_path, dapthe_data_temp, cmap="turbo")
    plt.close(fig)


def depth_visual(depth_data):
    """
    visualize the depth map
    """
    min = np.min(depth_data)
    max = np.max(depth_data)
    norm = mpl.colors.Normalize(vmin=min, vmax=max)
    cmap = plt.get_cmap('jet')

    m = cm.ScalarMappable(norm=norm, cmap=cmap)
    return (m.to_rgba(depth_data)[:, :, :3] * 255).astype(np.uint8)


def read_dpt(dpt_file_path):
    """read depth map from *.dpt file.

    :param dpt_file_path: the dpt file path
    :type dpt_file_path: str
    :return: depth map data
    :rtype: numpy
    """
    TAG_FLOAT = 202021.25  # check for this when READING the file

    ext = os.path.splitext(dpt_file_path)[1]

    assert len(ext) > 0, ('readFlowFile: extension required in fname %s' % dpt_file_path)
    assert ext == '.dpt', exit('readFlowFile: fname %s should have extension ''.flo''' % dpt_file_path)

    fid = None
    try:
        fid = open(dpt_file_path, 'rb')
    except IOError:
        print('readFlowFile: could not open %s', dpt_file_path)

    tag = unpack('f', fid.read(4))[0]
    width = unpack('i', fid.read(4))[0]
    height = unpack('i', fid.read(4))[0]

    assert tag == TAG_FLOAT, ('readFlowFile(%s): wrong tag (possibly due to big-endian machine?)' % dpt_file_path)
    assert 0 < width and width < 100000, ('readFlowFile(%s): illegal width %d' % (dpt_file_path, width))
    assert 0 < height and height < 100000, ('readFlowFile(%s): illegal height %d' % (dpt_file_path, height))

    # arrange into matrix form
    depth_data = np.fromfile(fid, np.float32)
    depth_data = depth_data.reshape(height, width)

    fid.close()

    return depth_data


def read_exr(exp_file_path):
    """Read depth map from EXR file

    :param exp_file_path: file path
    :type exp_file_path: str
    :return: depth map data
    :rtype: numpy 
    """
    import array
    import OpenEXR
    import Imath

    # Open the input file
    file = OpenEXR.InputFile(exp_file_path)

    # Compute the size
    dw = file.header()['dataWindow']
    sz = (dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1)

    # Read the three color channels as 32-bit floats
    FLOAT = Imath.PixelType(Imath.PixelType.FLOAT)
    (R, G, B) = [array.array('f', file.channel(Chan, FLOAT)).tolist() for Chan in ("R", "G", "B")]
    # R,G,B channel is same
    R_np = np.array(R).reshape((sz[1], sz[0]))

    return R_np


def read_pfm(path):
    """Read pfm file.

    :param path: the PFM file's path.
    :type path: str
    :return: the depth map array and scaler of depth
    :rtype: tuple: (data, scale)
    """
    with open(path, "rb") as file:

        color = None
        width = None
        height = None
        scale = None
        endian = None

        header = file.readline().rstrip()
        if header.decode("ascii") == "PF":
            color = True
        elif header.decode("ascii") == "Pf":
            color = False
        else:
            log.error("Not a PFM file: " + path)

        dim_match = re.match(r"^(\d+)\s(\d+)\s$", file.readline().decode("ascii"))
        if dim_match:
            width, height = list(map(int, dim_match.groups()))
        else:
            log.error("Malformed PFM header.")

        scale = float(file.readline().decode("ascii").rstrip())
        if scale < 0:
            # little-endian
            endian = "<"
            scale = -scale
        else:
            # big-endian
            endian = ">"

        data = np.fromfile(file, endian + "f")
        shape = (height, width, 3) if color else (height, width)

        data = np.reshape(data, shape)
        data = np.flipud(data)

        return data, scale


def write_pfm(path, image, scale=1):
    """Write depth data to pfm file.

    :param path: pfm file path
    :type path: str
    :param image: depth data
    :type image: numpy
    :param scale: Scale, defaults to 1
    :type scale: int, optional
    """
    if image.dtype.name != "float32":
        #raise Exception("Image dtype must be float32.")
        log.warn("The depth map data is {}, convert to float32 and save to pfm format.".format(image.dtype.name))
        image_ = image.astype(np.float32)
    else :
        image_ = image

    image_ = np.flipud(image_)

    color = None
    if len(image_.shape) == 3 and image_.shape[2] == 3:  # color image
        color = True
    elif len(image_.shape) == 2 or len(image_.shape) == 3 and image_.shape[2] == 1:  # greyscale
        color = False
    else:
        log.error("Image must have H x W x 3, H x W x 1 or H x W dimensions.")
        # raise Exception("Image must have H x W x 3, H x W x 1 or H x W dimensions.")

    with open(path, "wb") as file:
        file.write("PF\n" if color else "Pf\n".encode())
        file.write("%d %d\n".encode() % (image_.shape[1], image_.shape[0]))
        endian = image_.dtype.byteorder
        if endian == "<" or endian == "=" and sys.byteorder == "little":
            scale = -scale
        file.write("%f\n".encode() % scale)
        image_.tofile(file)


def depthmap_histogram(data):
    """
    Visualize the pixel value distribution.
    """
    data_max = int(np.max(data))
    data_min = int(np.min(data) + 0.5)
    big_number = 40 #int((data_max - data_min + 1) / 10000)
    print("depth map max is {}, min is {}, bin nubmer is {}".format(np.max(data), np.min(data), big_number))
    bin_range = np.linspace(data_min, data_max+1, big_number)
    histogram, bin_edges = np.histogram(data, bin_range, range=(np.min(data), np.max(data)))
    # configure and draw the histogram figure
    plt.figure()
    plt.title("Depth map data distribution.")
    plt.xlabel("depth value")
    plt.ylabel("pixels")
    plt.plot(bin_edges[0:-1], histogram)  # <- or here
    plt.show()


def depth2disparity(depth_map, baseline=1.0, focal=1.0):
    """
    Convert the depth map to disparity map.

    :param depth_map: depth map data
    :type depth_map: numpy
    :param baseline: [description], defaults to 1
    :type baseline: float, optional
    :param focal: [description], defaults to 1
    :type focal: float, optional
    :return: disparity map data, 
    :rtype: numpy
    """
    no_zeros_index = np.where(depth_map != 0)
    disparity_map = np.full(depth_map.shape, np.Inf, np.float64)
    disparity_map[no_zeros_index] = (baseline * focal) / depth_map[no_zeros_index]
    return disparity_map


def disparity2depth(disparity_map,  baseline=1.0, focal=1.0):
    """Convert disparity value to depth value.
    """
    no_zeros_index = np.where(disparity_map != 0)
    depth_map = np.full(disparity_map.shape, np.Inf, np.float64)
    depth_map[no_zeros_index] = (baseline * focal) / disparity_map[no_zeros_index]
    return depth_map
