/* DarkHelp - C++ helper class for Darknet's C API.
 * Copyright 2019-2021 Stephane Charette <stephanecharette@gmail.com>
 * MIT license applies.  See "license.txt" for details.
 */

#pragma once

// Do not include this header file directly.  Instead, your project should include DarkHelp.hpp
// which will then include all of the necessary secondary headers in the correct order.

#include <DarkHelp.hpp>

/** @file
 * DarkHelp's utility functions.
 */


namespace DarkHelp
{
	/// Get a version string for the %DarkHelp library.  E.g., could be `1.0.0-123`.
	std::string version();

	/** Format a duration as a text string which is typically added to images or video frames during annotation.
	 * For example, this might return @p "912 microseconds" or @p "375 milliseconds".
	 * @see @ref DarkHelp::NN::annotate()
	 */
	std::string duration_string(const std::chrono::high_resolution_clock::duration duration);

	/** Obtain a vector of at least 25 different bright colours that may be used to annotate images.  OpenCV uses BGR, not RGB.
	 * For example:
	 *
	 * @li @p "{0, 0, 255}" is pure red
	 * @li @p "{255, 0, 0}" is pure blue
	 *
	 * The colours returned by this function are intended to be used by OpenCV, and thus are in BGR format.
	 * @see @ref DarkHelp::Config::annotation_colours
	 *
	 * Default colours returned by this method are:
	 *
	 * Index	| RGB Hex	| Name
	 * ---------|-----------|-----
	 * 0		| @p FF355E	| <span style="background-color: #FF355E">Radical Red</span>
	 * 1		| @p 299617	| <span style="background-color: #299617">Slimy Green</span>
	 * 2		| @p FFCC33	| <span style="background-color: #FFCC33">Sunglow</span>
	 * 3		| @p AF6E4D	| <span style="background-color: #AF6E4D">Brown Sugar</span>
	 * 4		| @p FF00FF	| <span style="background-color: #FF00FF">Pure magenta</span>
	 * 5		| @p 50BFE6	| <span style="background-color: #50BFE6">Blizzard Blue</span>
	 * 6		| @p CCFF00	| <span style="background-color: #CCFF00">Electric Lime</span>
	 * 7		| @p 00FFFF	| <span style="background-color: #00FFFF">Pure cyan</span>
	 * 8		| @p 8D4E85	| <span style="background-color: #8D4E85">Razzmic Berry</span>
	 * 9		| @p FF48CC	| <span style="background-color: #FF48CC">Purple Pizzazz</span>
	 * 10		| @p 00FF00	| <span style="background-color: #00FF00">Pure green</span>
	 * 11		| @p FFFF00	| <span style="background-color: #FFFF00">Pure yellow</span>
	 * 12		| @p 5DADEC	| <span style="background-color: #5DADEC">Blue Jeans</span>
	 * 13		| @p FF6EFF	| <span style="background-color: #FF6EFF">Shocking Pink</span>
	 * 14		| @p AAF0D1	| <span style="background-color: #AAF0D1">Magic Mint</span>
	 * 15		| @p FFC000	| <span style="background-color: #FFC000">Orange</span>
	 * 16		| @p 9C51B6	| <span style="background-color: #9C51B6">Purple Plum</span>
	 * 17		| @p FF9933	| <span style="background-color: #FF9933">Neon Carrot</span>
	 * 18		| @p 66FF66	| <span style="background-color: #66FF66">Screamin' Green</span>
	 * 19		| @p FF0000	| <span style="background-color: #FF0000">Pure red</span>
	 * 20		| @p 4B0082	| <span style="background-color: #4B0082">Indigo</span>
	 * 21		| @p FF6037	| <span style="background-color: #FF6037">Outrageous Orange</span>
	 * 22		| @p FFFF66	| <span style="background-color: #FFFF66">Laser Lemon</span>
	 * 23		| @p FD5B78	| <span style="background-color: #FD5B78">Wild Watermelon</span>
	 * 24		| @p 0000FF	| <span style="background-color: #0000FF">Pure blue</span>
	 */
	VColours get_default_annotation_colours();

	/** Look at the names and/or the contents of all 3 files and swap the filenames around if necessary so the @p .cfg,
	 * @p .weights, and @p .names are assigned where they should be.  This is necessary because darknet tends to segfault
	 * if it is given the wrong filename.  (For example, if it mistakenly tries to parse the @p .weights file as a @p .cfg
	 * file.)  This function does a bit of sanity checking, determines which file is which, and also returns a map of debug
	 * information related to each file.
	 *
	 * On @em input, it doesn't matter which file goes into which parameter.  Simply pass in the filenames in any order.
	 *
	 * On @em output, the @p .cfg, @p .weights, and @p .names will be set correctly.  If needed for display purposes, some
	 * additional information is also passed back using the @p MStr string map, but most callers should ignore this output.
	 *
	 * @see @ref DarkHelp::NN::init()
	 */
	MStr verify_cfg_and_weights(std::string & cfg_filename, std::string & weights_filename, std::string & names_filename);

	/** This is used to insert lines into the @p [net] section of the configuration file.  Pass in a map of key-value pairs,
	 * and if the key exists it will be modified.  If the key does not exist, then it will be added to the bottom of the
	 * @p [net] section.
	 *
	 * For example, this is used by @ref DarkHelp::NN::init() when @ref DarkHelp::Config::modify_batch_and_subdivisions
	 * is enabled.
	 *
	 * @returns The number of lines that were modified or had to be inserted into the configuration file.
	 */
	size_t edit_cfg_file(const std::string & cfg_filename, MStr m);

	/** Automatically called by @ref DarkHelp::NN::predict_internal() when @ref DarkHelp::Config::fix_out_of_bound_values
	 * has been set.
	 */
	void fix_out_of_bound_normalized_rect(float & cx, float & cy, float & w, float & h);

	/** Convenience function to resize an image yet retain the exact original aspect ratio.  Performs no resizing if the
	 * image is already the desired size.  Depending on the size of the original image and the desired size, a "best"
	 * size will be chosen that <b>does not exceed</b> the specified size.  No letterboxing will be performed.
	 *
	 * For example, if the image is 640x480, and the specified size is 400x400, the image returned will be 400x300
	 * which maintains the original 1.333 aspect ratio.
	 */
	cv::Mat resize_keeping_aspect_ratio(cv::Mat mat, const cv::Size & desired_size);

	/** Resize the given image as quickly as possible to the given dimensions.  This will sacrifice quality for speed.
	 * If OpenCV has been compiled with support for CUDA, then this will utilise the GPU to do the resizing.
	 *
	 * @note Timing tests on Jetson devices as well as full NVIDIA GPUs show this is not significantly different than the
	 * usual call to @p cv::resize().  Probably would be of bigger impact if the image resizing was done on a different
	 * thread, and then fed to DarkHelp for inference so the image resize and inference can happen in parallel.
	 */
	cv::Mat fast_resize_ignore_aspect_ratio(const cv::Mat & mat, const cv::Size & desired_size);

#ifdef DARKHELP_CAN_INCLUDE_DARKNET
	/** Function to convert the OpenCV @p cv::Mat objects to Darknet's internal @p image format.
	 * Provided for convenience in case you need to call into one of Darknet's functions.
	 * @see @ref DarkHelp::convert_darknet_image_to_opencv_mat()
	 */
	image convert_opencv_mat_to_darknet_image(cv::Mat mat);

	/** Function to convert Darknet's internal @p image format to OpenCV's @p cv::Mat format.
	 * Provided for convenience in case you need to manipulate a Darknet image.
	 * @see @ref DarkHelp::convert_opencv_mat_to_darknet_image()
	 */
	cv::Mat convert_darknet_image_to_opencv_mat(const image img);
#endif
 };
