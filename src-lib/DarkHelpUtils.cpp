/* DarkHelp - C++ helper class for Darknet's C API.
 * Copyright 2019-2021 Stephane Charette <stephanecharette@gmail.com>
 * MIT license applies.  See "license.txt" for details.
 */

#include <DarkHelp.hpp>
//#include <fstream>
#include <regex>
//#include <cmath>
//#include <ctime>
#include <sys/stat.h>

#ifdef HAVE_OPENCV_CUDAWARPING
#include <opencv2/cudawarping.hpp>
#endif


#if 0
/* OpenCV4 has renamed some common defines and placed them in the cv namespace.  Need to deal with this until older
 * versions of OpenCV are no longer in use.
 */
#if 1 /// @todo remove this soon
#ifndef CV_INTER_CUBIC
#define CV_INTER_CUBIC cv::INTER_CUBIC
#endif
#ifndef CV_INTER_AREA
#define CV_INTER_AREA cv::INTER_AREA
#endif
#ifndef CV_AA
#define CV_AA cv::LINE_AA
#endif
#ifndef CV_FILLED
#define CV_FILLED cv::FILLED
#endif
#endif
#endif


std::string DarkHelp::version()
{
	return DH_VERSION;
}


std::string DarkHelp::duration_string(const std::chrono::high_resolution_clock::duration duration)
{
	const auto length_in_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
	const auto length_in_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration - length_in_milliseconds);

	std::stringstream ss;
	ss	<< length_in_milliseconds.count()
		<< "." << std::setw(3) << std::setfill('0')
		<< length_in_microseconds.count()
		<< " milliseconds";

	return ss.str();
}


DarkHelp::VColours DarkHelp::get_default_annotation_colours()
{
	VColours colours =
	{
		// remember the OpenCV format is blue-green-red and not RGB!
		{0x5E, 0x35, 0xFF},	// Radical Red
		{0x17, 0x96, 0x29},	// Slimy Green
		{0x33, 0xCC, 0xFF},	// Sunglow
		{0x4D, 0x6E, 0xAF},	// Brown Sugar
		{0xFF, 0x00, 0xFF},	// pure magenta
		{0xE6, 0xBF, 0x50},	// Blizzard Blue
		{0x00, 0xFF, 0xCC},	// Electric Lime
		{0xFF, 0xFF, 0x00},	// pure cyan
		{0x85, 0x4E, 0x8D},	// Razzmic Berry
		{0xCC, 0x48, 0xFF},	// Purple Pizzazz
		{0x00, 0xFF, 0x00},	// pure green
		{0x00, 0xFF, 0xFF},	// pure yellow
		{0xEC, 0xAD, 0x5D},	// Blue Jeans
		{0xFF, 0x6E, 0xFF},	// Shocking Pink
		{0xD1, 0xF0, 0xAA},	// Magic Mint
		{0x00, 0xC0, 0xFF},	// orange
		{0xB6, 0x51, 0x9C},	// Purple Plum
		{0x33, 0x99, 0xFF},	// Neon Carrot
		{0x66, 0xFF, 0x66},	// Screamin' Green
		{0x00, 0x00, 0xFF},	// pure red
		{0x82, 0x00, 0x4B},	// Indigo
		{0x37, 0x60, 0xFF},	// Outrageous Orange
		{0x66, 0xFF, 0xFF},	// Laser Lemon
		{0x78, 0x5B, 0xFD},	// Wild Watermelon
		{0xFF, 0x00, 0x00}	// pure blue
	};

	return colours;
}


DarkHelp::MStr DarkHelp::verify_cfg_and_weights(std::string & cfg_filename, std::string & weights_filename, std::string & names_filename)
{
	MStr m;

	// we need a minimum of 2 unique files for things to be valid
	std::set<std::string> all_filenames;
	all_filenames.insert(cfg_filename		);
	all_filenames.insert(weights_filename	);
	all_filenames.insert(names_filename		);
	if (all_filenames.size() < 2)
	{
		/// @throw std::invalid_argument if at least 2 unique filenames have not been provided
		throw std::invalid_argument("need a minimum of 2 filenames (cfg and weights) to load darknet neural network");
	}

	// The simplest case is to look at the file extensions.  If they happen to be .cfg, and .weights, then
	// that is what we'll use to decide which file is which.  (And anything left over will be the .names file.)
	for (const auto & filename : all_filenames)
	{
		const size_t pos = filename.rfind(".");
		if (pos != std::string::npos)
		{
			const std::string ext = filename.substr(pos + 1);
			m[ext] = filename;
		}
	}
	if (m.count("cfg"		) == 1 and
		m.count("weights"	) == 1)
	{
		for (auto iter : m)
		{
			if (iter.first == "weights")	weights_filename	= iter.second;
			else if (iter.first == "cfg")	cfg_filename		= iter.second;
			else							names_filename		= iter.second;
		}

		// We *know* we have a cfg and weights, but the names is optional.  If we have a 3rd filename, then it must be the names,
		// otherwise blank out the 3rd filename in case that field was used to pass in either the cfg or the weights.
		if (names_filename == cfg_filename or names_filename == weights_filename)
		{
			names_filename = "";
		}
		m["names"] = names_filename;
	}
	else
	{
		// If we get here, then the filenames are not obvious just by looking at the extensions.
		//
		// Instead, we're going to use the size of the files to decide what is the .cfg, .weights, and .names.  The largest
		// file (MiB) will always be the .weights.  The next one (KiB) will be the configuration.  And the names, if available,
		// is only a few bytes in size.  Because the order of magnitude is so large, the three files should never have the
		// exact same size unless something has gone very wrong.

		m.clear();

		std::map<size_t, std::string> file_size_map;
		for (const auto & filename : all_filenames)
		{
			struct stat buf;
			buf.st_size = 0; // only field we actually use is the size, so initialize it to zero in case the stat() call fails
			stat(filename.c_str(), &buf);
			file_size_map[buf.st_size] = filename;
		}

		if (file_size_map.size() != 3)
		{
			/// @throw std::runtime_error if the size of the files cannot be determined (one or more file does not exist?)
			throw std::runtime_error("cannot access .cfg or .weights file");
		}

		// iterate through the files from smallest up to the largest
		auto iter = file_size_map.begin();

		names_filename		= iter->second;
		m["names"]			= iter->second;
		m[iter->second]		= std::to_string(iter->first) + " bytes";

		iter ++;
		cfg_filename		= iter->second;
		m["cfg"]			= iter->second;
		m[iter->second]		= std::to_string(iter->first) + " bytes";

		iter ++;
		weights_filename	= iter->second;
		m["weights"]		= iter->second;
		m[iter->second]		= std::to_string(iter->first) + " bytes";
	}

	// now that we know which file is which, read the first few bytes or lines to see if it contains what we'd expect

	std::ifstream ifs;

	// look for "[net]" within the first few lines of the .cfg file
	ifs.open(cfg_filename);
	if (ifs.is_open() == false)
	{
		/// @throw std::invalid_argument if the cfg file doesn't exist
		throw std::invalid_argument("failed to open the configuration file " + cfg_filename);
	}
	bool found = false;
	for (size_t line_counter = 0; line_counter < 20; line_counter ++)
	{
		std::string line;
		std::getline(ifs, line);
		if (line.find("[net]") != std::string::npos)
		{
			found = true;
			break;
		}
	}
	if (not found)
	{
		/// @throw std::invalid_argument if the cfg file doesn't contain @p "[net]" near the top of the file
		throw std::invalid_argument("failed to find [net] section in configuration file " + cfg_filename);
	}

	// keep looking until we find "classes=###" so we know how many lines the .names file should have
	int number_of_classes = 0;
	const std::regex rx("^classes[ \t]*=[ \t]*([0-9]+)");
	while (ifs.good())
	{
		std::string line;
		std::getline(ifs, line);
		std::smatch sm;
		if (std::regex_search(line, sm, rx))
		{
			m["number of classes"] = sm[1].str();
			number_of_classes = std::stoi(sm[1].str());
			break;
		}
	}

	if (number_of_classes < 1)
	{
		/// @throw std::invalid_argument if the configuration file does not have a line that says "classes=..."
		throw std::invalid_argument("failed to find the number of classes in the configuration file " + cfg_filename);
	}

	// first 4 fields in the weights file -- see save_weights_upto() in darknet's src/parser.c
	ifs.close();
	ifs.open(weights_filename, std::ifstream::in | std::ifstream::binary);
	if (ifs.is_open() == false)
	{
		/// @throw std::invalid_argument if the weights file doesn't exist
		throw std::invalid_argument("failed to open the weights file " + weights_filename);
	}
	uint32_t major	= 0;
	uint32_t minor	= 0;
	uint32_t patch	= 0;
	uint64_t seen	= 0;
	ifs.read(reinterpret_cast<char*>(&major	), sizeof(major	));
	ifs.read(reinterpret_cast<char*>(&minor	), sizeof(minor	));
	ifs.read(reinterpret_cast<char*>(&patch	), sizeof(patch	));
	ifs.read(reinterpret_cast<char*>(&seen	), sizeof(seen	));
	m["weights major"	] = std::to_string(major);
	m["weights minor"	] = std::to_string(minor);
	m["weights patch"	] = std::to_string(patch);
	m["images seen"		] = std::to_string(seen);

	if (major * 10 + minor < 2)
	{
		/// @throw std::invalid_argument if weights file has an invalid version number (or weights file is from an extremely old version of darknet?)
		throw std::invalid_argument("failed to find the version number in the weights file " + weights_filename);
	}

	if (names_filename.empty() == false)
	{
		ifs.close();
		ifs.open(names_filename);
		std::string line;
		int line_counter = 0;
		while (std::getline(ifs, line))
		{
			line_counter ++;
		}
		m["number of names"] = std::to_string(line_counter);

		if (line_counter != number_of_classes)
		{
			/// @throw std::runtime_error if the number of lines in the names file doesn't match the number of classes in the configuration file
			throw std::runtime_error("the network configuration defines " + std::to_string(number_of_classes) + " classes, but the file " + names_filename + " has " + std::to_string(line_counter) + " lines");
		}
	}

	return m;
}


size_t DarkHelp::edit_cfg_file(const std::string & cfg_filename, DarkHelp::MStr m)
{
	if (m.empty())
	{
		// nothing to do!
		return 0;
	}

	std::ifstream ifs(cfg_filename);
	if (not ifs.is_open())
	{
		/// @throw std::invalid_argument if the cfg file does not exist or cannot be opened
		throw std::invalid_argument("failed to open the configuration file " + cfg_filename);
	}

	// read the file and look for the [net] section
	bool net_section_found	= false;
	size_t net_idx_start	= 0;
	size_t net_idx_end		= 0;
	VStr v;
	std::string cfg_line;
	while (std::getline(ifs, cfg_line))
	{
		// if the .cfg has DOS-style \r\n line endings and we're reading
		// the file in linux, then expect the string to be "[net]\r"
		if (cfg_line.size() >= 5 and cfg_line.substr(0, 5) == "[net]")
		{
			net_idx_start	= v.size();
			net_idx_end		= v.size();
			net_section_found = true;
		}
		else if (cfg_line.size() >= 3)
		{
			if (net_section_found == true)
			{
				if (net_idx_end == net_idx_start)
				{
					if (cfg_line[0] == '[')
					{
						// we found the start of a new section, so this must mean the end of [net] has been found
						net_idx_end = v.size();
					}
				}
			}
		}

		v.push_back(cfg_line);
	}
	ifs.close();

	if (net_idx_start == net_idx_end)
	{
		/// @throw std::runtime_error if a valid start and end to the [net] section wasn't found in the .cfg file
		throw std::runtime_error("failed to properly identify the [net] section in " + cfg_filename);
	}

	// look at every line in the [net] section to see if it matches one of the keys we want to modify
	const std::regex rx(
		"^"				// start of text
		"\\s*"			// consume all whitespace
		"("				// group #1
			"[^#=\\s]+"	// not "#", "=", or whitespace
		")"
		"\\s*"			// consume all whitespace
		"="				// "="
		"\\s*"			// consume all whitespace
		"("				// group #2
			".*"		// old value and any trailing text (e.g., comments)
		")"
		"$"				// end of text
	);

	bool initial_modification = false;
	if (m.size()				== 2	and
		m.count("batch")		== 1	and
		m.count("subdivisions")	== 1	and
		m["batch"]				== "1"	and
		m["subdivisions"]		== "1"	)
	{
		// we need to know if this is the initial batch/subdivisions modification performed by init()
		// because there are cases were we'll need to abort modifying the .cfg file if this is the case
		initial_modification = true;
	}

	size_t number_of_changed_lines = 0;
	for (size_t idx = net_idx_start; idx < net_idx_end; idx ++)
	{
		std::string & line = v[idx];

		std::smatch sm;
		if (std::regex_match(line, sm, rx))
		{
			const std::string key = sm[1].str();
			const std::string val = sm[2].str();

			if (key						== "contrastive"	and
				val						== "1"				and
				initial_modification	== true				)
			{
				// this is one of the new configuration files that uses "contrastive", so don't modify "batch" and "subdivisions" so we
				// can avoid the darknet error about "mini_batch size (batch/subdivisions) should be higher than 1 for Contrastive loss"
				return 0;
			}

			// now see if this key is one of the ones we want to modify
			if (m.count(key) == 1)
			{
				if (val != m.at(key))
				{
					line = key + "=" + m.at(key);
					number_of_changed_lines ++;
				}
				m.erase(key);
			}
		}
	}

	// whatever is left in the map at this point needs to be inserted at the end of the [net] section (must be a new key)
	for (auto iter : m)
	{
		const std::string & key = iter.first;
		const std::string & val = iter.second;
		const std::string line = key + "=" + val;

		v.insert(v.begin() + net_idx_end, line);
		number_of_changed_lines ++;
		net_idx_end ++;
	}

	if (number_of_changed_lines == 0)
	{
		// nothing to do, no need to re-write the .cfg file
		return 0;
	}

	// now we need to re-create the .cfg file
	const std::string tmp_filename = cfg_filename + "_TMP";
	std::ofstream ofs(tmp_filename);
	if (not ofs.is_open())
	{
		/// @throw std::runtime_error if we cannot write a new .cfg file
		throw std::runtime_error("failed to save changes to .cfg file " + tmp_filename);
	}
	for (const auto & line : v)
	{
		ofs << line << std::endl;
	}
	ofs.close();
	std::remove(cfg_filename.c_str());	// Windows seems to require we remove the file before we can rename
	const int result = std::rename(tmp_filename.c_str(), cfg_filename.c_str());
	if (result)
	{
		/// @throw std::runtime_error if we cannot rename the .cfg file
		throw std::runtime_error("failed to overwrite .cfg file " + cfg_filename);
	}

	return number_of_changed_lines;
}


void DarkHelp::fix_out_of_bound_normalized_rect(float & cx, float & cy, float & w, float & h)
{
	// coordinates are all normalized!

	if (cx - w / 2.0f < 0.0f ||	// too far left
		cx + w / 2.0f > 1.0f)	// too far right
	{
		// calculate a new X and width to use for this prediction
		const float new_x1 = std::max(0.0f, cx - w / 2.0f);
		const float new_x2 = std::min(1.0f, cx + w / 2.0f);
		const float new_w = new_x2 - new_x1;
		const float new_x = (new_x1 + new_x2) / 2.0f;
		cx = new_x;
		w = new_w;
	}

	if (cy - h / 2.0f < 0.0f ||	// too far above
		cy + h / 2.0f > 1.0f)	// too far below
	{
		// calculate a new Y and height to use for this prediction
		const float new_y1 = std::max(0.0f, cy - h / 2.0f);
		const float new_y2 = std::min(1.0f, cy + h / 2.0f);
		const float new_h = new_y2 - new_y1;
		const float new_y = (new_y1 + new_y2) / 2.0f;
		cy = new_y;
		h = new_h;
	}

	return;
}


cv::Mat DarkHelp::resize_keeping_aspect_ratio(cv::Mat mat, const cv::Size & desired_size)
{
	if (mat.empty())
	{
		return mat;
	}

	if (desired_size.width == mat.cols and desired_size.height == mat.rows)
	{
		return mat;
	}

	if (desired_size.width < 1 or desired_size.height < 1)
	{
		// return an empty image
		return cv::Mat();
	}

	const double image_width		= static_cast<double>(mat.cols);
	const double image_height		= static_cast<double>(mat.rows);
	const double horizontal_factor	= image_width	/ static_cast<double>(desired_size.width);
	const double vertical_factor	= image_height	/ static_cast<double>(desired_size.height);
	const double largest_factor 	= std::max(horizontal_factor, vertical_factor);
	const double new_width			= image_width	/ largest_factor;
	const double new_height			= image_height	/ largest_factor;
	const cv::Size new_size(std::round(new_width), std::round(new_height));

	// "To shrink an image, it will generally look best with CV_INTER_AREA interpolation ..."
	auto interpolation = CV_INTER_AREA;
	if (largest_factor < 1.0)
	{
		// "... to enlarge an image, it will generally look best with CV_INTER_CUBIC"
		interpolation = CV_INTER_CUBIC;
	}

	cv::Mat dst;
	cv::resize(mat, dst, new_size, 0, 0, interpolation);

	return dst;
}


cv::Mat DarkHelp::fast_resize_ignore_aspect_ratio(const cv::Mat & mat, const cv::Size & desired_size)
{
	if (mat.size() == desired_size or mat.empty())
	{
		return mat;
	}

	cv::Mat resized_image;

	// INTER_NEAREST is the fastest resize method at a cost of quality, but since we're making the image match the
	// network dimensions (416x416, etc) we're aiming for speed, not quality.  Truth is when saving the file out to
	// disk as a .png file during testing I noticed the output is *exactly* the same.  MD5 sum of the image files
	// was exactly the same regardless of the method used.  So we may as well use the fastest method available.

	#ifdef HAVE_OPENCV_CUDAWARPING

		// if we get here, we'll use the GPU to do the resizing

		static thread_local cv::cuda::GpuMat gpu_original_image;
		static thread_local cv::cuda::GpuMat gpu_resized_image;

		gpu_original_image.upload(mat);
		cv::cuda::resize(gpu_original_image, gpu_resized_image, desired_size, 0.0, 0.0, cv::INTER_NEAREST);
		gpu_resized_image.download(resized_image);

	#else

		// otherwise we don't have a GPU to use so call the "normal" CPU version of cv::resize()

		cv::resize(mat, resized_image, desired_size, cv::INTER_NEAREST);

	#endif

	return resized_image;
}
