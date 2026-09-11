#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_
/**
 * \file executor.h
 *
 * Copyright (C) 2015 Andreas Altair Redmer, <altair.ibn.la.ahad.sy@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of one of the following licenses:
 *
 * \li 1. GNU Lesser General Public License, version 2.1 (see LICENSE-LGPL)
 * \li 2. GNU General Public License, version 2  (see LICENSE-GPL)
 *
 * If you want to help with choosing the best license for you,
 * please visit http://www.gnu.org/licenses/license-list.html.
 *
 */

#include <dirent.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

class Executor
{
	/// Maximum directory nesting depth to descend into
	/// (guards against stack exhaustion on pathological trees)
	static const int MAX_DEPTH = 1024;

	private:
		static bool hasDotComponent(const std::string& path)
		{
			size_t start = 0;
			while (start < path.length()) {
				size_t end = path.find('/', start);
				if (end == std::string::npos)
					end = path.length();
				if (end > start && path[start] == '.')
					return true;
				start = end + 1;
			}
			return false;
		}

		static bool shouldInclude(const std::string& path, bool includeDotDirs)
		{
			return includeDotDirs || !hasDotComponent(path);
		}

		static void collectPaths(const std::string& path, bool includeDotDirs, bool dirsOnly, std::set<std::string>& out)
		{
			// iterative traversal with an explicit stack - recursion
			// would overflow the stack on deeply nested trees
			std::vector<std::pair<std::string, int> > stack;
			stack.push_back(std::make_pair(path, 0));

			while (!stack.empty()) {
				const std::string current = stack.back().first;
				const int depth = stack.back().second;
				stack.pop_back();

				struct stat st;
				if (lstat(current.c_str(), &st) != 0)
					continue;

				const bool isDir = S_ISDIR(st.st_mode);
				if ((!dirsOnly || isDir) && shouldInclude(current, includeDotDirs))
					out.insert(current);

				if (!isDir || depth >= MAX_DEPTH)
					continue;

				DIR* dir = opendir(current.c_str());
				if (dir == NULL)
					continue;

				struct dirent* entry = NULL;
				while ((entry = readdir(dir)) != NULL) {
					const std::string name(entry->d_name);
					if (name == "." || name == "..")
						continue;
					if (!includeDotDirs && !name.empty() && name[0] == '.')
						continue;
					stack.push_back(std::make_pair(current + "/" + name, depth + 1));
				}

				closedir(dir);
			}
		}

		static std::vector<std::string> expandDescriptor(const std::string& path, bool includeDotDirs, bool dirsOnly)
		{
			std::set<std::string> collected;
			glob_t matches;
			const int res = glob(path.c_str(), GLOB_NOSORT, NULL, &matches);
			if (res == 0) {
				for (size_t i=0; i<matches.gl_pathc; ++i)
					collectPaths(matches.gl_pathv[i], includeDotDirs, dirsOnly, collected);
				globfree(&matches);
			}
			else if (res == GLOB_NOMATCH) {
				globfree(&matches);
				collectPaths(path, includeDotDirs, dirsOnly, collected);
			}
			else {
				globfree(&matches);
			}

			return std::vector<std::string>(collected.begin(), collected.end());
		}

	public:
		/**
		 * Returns all subdirectories of the directory descriptor as vector of strings.
		 */
		static const std::vector<std::string> getSubDirVec(std::string dir, bool includeDotDirs=false)
		{
			return expandDescriptor(dir, includeDotDirs, true);
		}

		/**
		 * Returns all files in case the file descriptor contains a star.
		 */
		static const std::vector<std::string> getAllFilesByDescriptor(std::string dir, bool includeDotDirs=false)
		{
			return expandDescriptor(dir, includeDotDirs, false);
		}
};

#endif //_EXECUTOR_H_
