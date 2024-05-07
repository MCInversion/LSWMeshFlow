#include "GeometryConversionUtils.h"

#include "utils/StringUtils.h"

#include <set>
#include <fstream>
#include <random>
#include <thread>
#include <unordered_set>

#include <nanoflann.hpp>

#include "pmp/algorithms/Normals.h"
#include "quickhull/QuickHull.hpp"

#ifdef _WINDOWS
// Windows-specific headers
#include <windows.h>
#include <fcntl.h>
#else
// Unsupported platform
#error "Unsupported platform"
#endif


namespace
{

	// TODO: different representations, e.g.: ProgressiveMeshData which would then be exported into a disk file
	/**
	 * \brief a thread-specific wrapper for mesh data.
	 */
	struct ChunkData
	{
		std::vector<pmp::vec3> Vertices;
		std::vector<pmp::vec3> VertexNormals;
		std::vector<std::vector<unsigned int>> PolyIndices;
	};

	/**
	 * \brief Parses a chunk of OBJ data. This function is run for each thread.
	 * \param start     start address for this thread.
	 * \param end       end address for this thread.
	 * \param data      preallocated chunk data.
	 */
	void ParseChunk(const char* start, const char* end, ChunkData& data)
	{
		const char* cursor = start;

		while (cursor < end)
		{
			// If the current line is incomplete, skip to the next line
			if (*cursor == '\n')
			{
				cursor++;
				continue;
			}

			// If it's a vertex or normal, parse the three floats
			if (strncmp(cursor, "v ", 2) == 0 || strncmp(cursor, "vn ", 3) == 0)
			{
				const bool isNormal = strncmp(cursor, "vn ", 3) == 0;
				cursor += isNormal ? 3 : 2; // skip "v " or "vn "

				pmp::vec3 vec;
				char* tempCursor;
				vec[0] = std::strtof(cursor, &tempCursor);
				cursor = tempCursor;

				vec[1] = std::strtof(cursor, &tempCursor);
				cursor = tempCursor;

				vec[2] = std::strtof(cursor, &tempCursor);
				cursor = tempCursor;

				if (isNormal)
					data.VertexNormals.push_back(vec);
				else
					data.Vertices.push_back(vec);
			}
			// If it's a face, parse the vertex indices
			else if (strncmp(cursor, "f ", 2) == 0)
			{
				cursor += 2; // skip "f "
				std::vector<unsigned int> faceIndices;

				while (*cursor != '\n' && cursor < end)
				{
					char* tempCursor;
					// Parse the vertex index
					const unsigned int vertexIndex = std::strtoul(cursor, &tempCursor, 10);
					if (cursor == tempCursor) // No progress in parsing, break to avoid infinite loop
						break;

					cursor = tempCursor;
					if (vertexIndex == 0) {
						// Underflow occurred, meaning strtoul failed to parse a valid number
						break;
					}
					faceIndices.push_back(vertexIndex - 1);

					if (*cursor == '/') // Check for additional indices
					{
						cursor++; // Skip the first '/'
						if (*cursor != '/') // Texture coordinate index is present
						{
							std::strtoul(cursor, &tempCursor, 10); // Parse and discard the texture index
							cursor = tempCursor;
						}

						if (*cursor == '/') // Normal index is present
						{
							cursor++; // Skip the second '/'
							std::strtoul(cursor, &tempCursor, 10); // Parse and discard the normal index
							cursor = tempCursor;
						}
					}

					// Skip to next index, newline, or end
					while (*cursor != ' ' && *cursor != '\n' && cursor < end)
						cursor++;

					if (*cursor == ' ')
						cursor++; // Skip space to start of next index
				}

				if (!faceIndices.empty())
					data.PolyIndices.push_back(faceIndices);

				// Move to the next line
				while (*cursor != '\n' && cursor < end)
					cursor++;
				if (cursor < end)
					cursor++; // Move past the newline character
			}
			else
			{
				// Skip to the next line if the current line isn't recognized
				while (*cursor != '\n' && cursor < end)
					cursor++;
			}
		}
	}

	// Assume average bytes per vertex and face line (these are just example values)
	//constexpr size_t avgBytesPerVertex = 13; // Example average size of a vertex line in bytes
	//constexpr size_t avgBytesPerFace = 7; // Example average size of a face line in bytes

	/**
	 * \brief A simple estimation utility for large OBJ files
	 * \param fileSize        file size in bytes.
	 * \param expectNormals   if true we expect the OBJ file to also contain vertex normals.
	 * \return pair { # of estimated vertices, # of estimated faces }.
	 */
	//std::pair<size_t, size_t> EstimateVertexAndFaceCapacitiesFromOBJFileSize(const size_t& fileSize, const bool& expectNormals = false)
	//{

	//	// Estimate total number of vertex and face lines
	//	const size_t estimatedTotalLines = fileSize / (((expectNormals ? 2 : 1) * avgBytesPerVertex + avgBytesPerFace) / 2);

	//	// Estimate number of vertices based on Botsch 2010 ratio N_F = 2 * N_V
	//	size_t estimatedVertices = estimatedTotalLines / (expectNormals ? 4 : 3); // Since N_F + N_V = 3 * N_V and N_V = N_N
	//	size_t estimatedFaces = 2 * estimatedVertices;

	//	return std::make_pair(estimatedVertices, estimatedFaces);
	//}

	/**
	 * \brief Parses a chunk of PLY point data. This function is run for each thread.
	 * \param start     start address for this thread.
	 * \param end       end address for this thread.
	 * \param data      preallocated chunk data.
	 */
	void ParsePointCloudChunk(const char* start, const char* end, std::vector<pmp::vec3>& data) {
		const char* cursor = start;

		while (cursor < end) 
		{
			// Skip any leading whitespace
			while ((*cursor == ' ' || *cursor == '\n') && cursor < end)
			{
				cursor++;
			}

			if (cursor >= end)
			{
				break; // Reached the end of the chunk
			}

			// Start of a new line
			const char* lineStart = cursor;

			// Find the end of the line
			while (*cursor != '\n' && cursor < end) 
			{
				cursor++;
			}

			// Check if the end of the line is within the chunk
			if (*cursor != '\n' && cursor == end) 
			{
				break; // The line is incomplete, so stop parsing
			}

			// Extract the line
			std::string line(lineStart, cursor);

			// Remove carriage return if present
			if (!line.empty() && line.back() == '\r') 
			{
				line.pop_back();
			}

			// Increment cursor to start the next line in the next iteration
			if (cursor < end) 
			{
				cursor++;
			}

			// Parse the line to extract vertex coordinates
			std::istringstream iss(line);
			pmp::vec3 vertex;
			if (!(iss >> vertex[0] >> vertex[1] >> vertex[2])) 
			{
				std::cerr << "ParsePointCloudChunk: Error parsing line: " << line << std::endl;
				continue;
			}

			data.push_back(vertex);
		}
	}

	/**
	 * \brief Reads the header of the PLY file for vertices.
	 * \param start     header start position in memory.
	 * \return pair { number of vertices read from the header, the memory position where the point data starts }
	 */
	[[nodiscard]] std::pair<size_t, char*> ReadPLYVertexHeader(const char* start)
	{
		const char* cursor = start;
		std::string line;
		size_t vertexCount = 0;

		while (*cursor != '\0') 
		{
			// Extract the line
			const char* lineStart = cursor;
			while (*cursor != '\n' && *cursor != '\0') 
			{
				cursor++;
			}
			line.assign(lineStart, cursor);

			// Trim trailing carriage return if present
			if (!line.empty() && line.back() == '\r') 
			{
				line.pop_back();
			}

			// Move to the start of the next line
			if (*cursor != '\0') 
			{
				cursor++;
			}

			// Check for the vertex count line
			if (line.rfind("element vertex", 0) == 0) 
			{
				std::istringstream iss(line);
				std::string element, vertex;
				if (!(iss >> element >> vertex >> vertexCount))
				{
					std::cerr << "ReadPLYVertexHeader [WARNING]: Failed to parse vertex count line: " << line << "\n";
					continue;
				}
				// Successfully parsed the vertex count line. Continue to look for the end of the header.
			}

			// Check for the end of the header
			if (line == "end_header") 
			{
				break;
			}
		}

		if (vertexCount == 0) 
		{
			std::cerr << "ReadPLYVertexHeader [ERROR]: Vertex count not found in the header.\n";
		}

		return { vertexCount, const_cast<char*>(cursor) }; // Return the vertex count and the cursor position
	}


} // anonymous namespace

namespace Geometry
{
	pmp::SurfaceMesh ConvertBufferGeomToPMPSurfaceMesh(const BaseMeshGeometryData& geomData)
	{
		pmp::SurfaceMesh result;

		// count edges
		std::set<std::pair<unsigned int, unsigned int>> edgeIdsSet;
		for (const auto& indexTuple : geomData.PolyIndices)
		{
			for (unsigned int i = 0; i < indexTuple.size(); i++)
			{
				unsigned int vertId0 = indexTuple[i];
				unsigned int vertId1 = indexTuple[(static_cast<size_t>(i) + 1) % indexTuple.size()];

				if (vertId0 > vertId1) std::swap(vertId0, vertId1);

				edgeIdsSet.insert({ vertId0, vertId1 });
			}
		}

		result.reserve(geomData.Vertices.size(), edgeIdsSet.size(), geomData.PolyIndices.size());
		for (const auto& v : geomData.Vertices)
			result.add_vertex(pmp::Point(v[0], v[1], v[2]));

		if (!geomData.VertexNormals.empty())
		{
			auto vNormal = result.vertex_property<pmp::Normal>("v:normal");
			for (auto v : result.vertices())
				vNormal[v] = geomData.VertexNormals[v.idx()];
		}

        for (const auto& indexTuple : geomData.PolyIndices)
        {
			std::vector<pmp::Vertex> vertices;
			vertices.reserve(indexTuple.size());
			for (const auto& vId : indexTuple)
				vertices.emplace_back(pmp::Vertex(vId));

			result.add_face(vertices);	        
        }

		return result;
	}

	BaseMeshGeometryData ConvertPMPSurfaceMeshToBaseMeshGeometryData(const pmp::SurfaceMesh& pmpMesh)
	{
		BaseMeshGeometryData geomData;

		// Extract vertices
		auto points = pmpMesh.get_vertex_property<pmp::Point>("v:point");
		for (const auto v : pmpMesh.vertices())
			geomData.Vertices.emplace_back(points[v][0], points[v][1], points[v][2]);

		// Extract vertex normals if available
		if (pmpMesh.has_vertex_property("v:normal")) 
		{
			auto normals = pmpMesh.get_vertex_property<pmp::Normal>("v:normal");
			for (const auto v : pmpMesh.vertices())
				geomData.VertexNormals.emplace_back(normals[v][0], normals[v][1], normals[v][2]);
		}

		// Extract face indices
		for (const auto f : pmpMesh.faces())
		{
			std::vector<unsigned int> faceIndices;
			for (auto v : pmpMesh.vertices(f))
			{
				faceIndices.push_back(v.idx());
			}
			geomData.PolyIndices.push_back(faceIndices);
		}

		return geomData;
	}

	pmp::SurfaceMesh ConvertMCMeshToPMPSurfaceMesh(const MarchingCubes::MC_Mesh& mcMesh)
	{
		pmp::SurfaceMesh result;

		// count edges
		std::set<std::pair<unsigned int, unsigned int>> edgeIdsSet;
		for (unsigned int i = 0; i < mcMesh.faceCount * 3; i += 3)
		{
			for (unsigned int j = 0; j < 3; j++)
			{
				unsigned int vertId0 = mcMesh.faces[i + j];
				unsigned int vertId1 = mcMesh.faces[i + (j + 1) % 3];

				if (vertId0 > vertId1) std::swap(vertId0, vertId1);

				edgeIdsSet.insert({ vertId0, vertId1 });
			}
		}

		result.reserve(mcMesh.vertexCount, edgeIdsSet.size(), mcMesh.faceCount);
		// MC produces normals by default
		auto vNormal = result.vertex_property<pmp::Normal>("v:normal");

		for (unsigned int i = 0; i < mcMesh.vertexCount; i++)
		{
			result.add_vertex(
			   pmp::Point(
				   mcMesh.vertices[i][0],
				   mcMesh.vertices[i][1],
				   mcMesh.vertices[i][2]
			));
			vNormal[pmp::Vertex(i)] = pmp::Normal{
				mcMesh.normals[i][0],
				mcMesh.normals[i][1],
				mcMesh.normals[i][2]
			};
		}

		for (unsigned int i = 0; i < mcMesh.faceCount * 3; i += 3)
		{
			std::vector<pmp::Vertex> vertices;
			vertices.reserve(3);
			for (unsigned int j = 0; j < 3; j++)
			{
				vertices.emplace_back(pmp::Vertex(mcMesh.faces[i + j]));
			}

			result.add_face(vertices);
		}

		return result;
	}

	bool ExportBaseMeshGeometryDataToOBJ(const BaseMeshGeometryData& geomData, const std::string& absFileName)
	{
		std::ofstream file(absFileName);
		if (!file.is_open())
		{
			std::cerr << "Failed to open file for writing: " << absFileName << std::endl;
			return false;
		}

		// Write vertices
		for (const auto& vertex : geomData.Vertices)
		{
			file << "v " << vertex[0] << ' ' << vertex[1] << ' ' << vertex[2] << '\n';
		}

		// Optionally, write vertex normals
		if (!geomData.VertexNormals.empty())
		{
			for (const auto& normal : geomData.VertexNormals)
			{
				file << "vn " << normal[0] << ' ' << normal[1] << ' ' << normal[2] << '\n';
			}
		}

		// Write faces
		for (const auto& indices : geomData.PolyIndices)
		{
			file << "f";
			for (unsigned int index : indices)
			{
				// OBJ indices start from 1, not 0
				file << ' ' << (index + 1);
			}
			file << '\n';
		}

		file.close();
		return true;
	}

	bool ExportBaseMeshGeometryDataToVTK(const BaseMeshGeometryData& geomData, const std::string& absFileName)
	{
		std::ofstream file(absFileName);
		if (!file.is_open())
		{
			std::cerr << "Failed to open file for writing: " << absFileName << std::endl;
			return false;
		}

		// Header
		file << "# vtk DataFile Version 3.0\n";
		file << "VTK output from mesh data\n";
		file << "ASCII\n";
		file << "DATASET POLYDATA\n";

		// Write vertices
		file << "POINTS " << geomData.Vertices.size() << " float\n";
		for (const auto& vertex : geomData.Vertices)
		{
			file << vertex[0] << ' ' << vertex[1] << ' ' << vertex[2] << '\n';
		}

		// Write polygons (faces)
		size_t numIndices = 0;
		for (const auto& indices : geomData.PolyIndices)
		{
			numIndices += indices.size() + 1;  // +1 because we also write the size of the polygon
		}
		file << "POLYGONS " << geomData.PolyIndices.size() << ' ' << numIndices << '\n';
		for (const auto& indices : geomData.PolyIndices)
		{
			file << indices.size();  // Number of vertices in this polygon
			for (unsigned int index : indices)
			{
				file << ' ' << index;  // VTK indices start from 0
			}
			file << '\n';
		}

		// Optionally, write vertex normals
		if (!geomData.VertexNormals.empty())
		{
			file << "POINT_DATA " << geomData.VertexNormals.size() << '\n';
			file << "NORMALS normals float\n";
			for (const auto& normal : geomData.VertexNormals)
			{
				file << normal[0] << ' ' << normal[1] << ' ' << normal[2] << '\n';
			}
		}

		file.close();
		return true;
	}

	std::optional<BaseMeshGeometryData> ImportOBJMeshGeometryData(const std::string& absFileName, const bool& importInParallel, std::optional<std::vector<float>*> chunkIdsVertexPropPtrOpt)
	{
		const auto extension = Utils::ExtractLowercaseFileExtensionFromPath(absFileName);
		if (extension != "obj")
			return {};

		const char* file_path = absFileName.c_str();

		const HANDLE file_handle = CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file_handle == INVALID_HANDLE_VALUE) 
		{
			std::cerr << "ImportOBJMeshGeometryData [ERROR]: Failed to open the file.\n";
			return {};
		}

		// Get the file size
		const DWORD file_size = GetFileSize(file_handle, nullptr);

		// Create a file mapping object
		const HANDLE file_mapping = CreateFileMapping(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (file_mapping == nullptr) 
		{
			std::cerr << "ImportOBJMeshGeometryData [ERROR]: Failed to create file mapping.\n";
			CloseHandle(file_handle);
			return {};
		}

		// Map the file into memory
		const LPVOID file_memory = MapViewOfFile(file_mapping, FILE_MAP_READ, 0, 0, 0);
		if (file_memory == nullptr) 
		{
			std::cerr << "ImportOBJMeshGeometryData [ERROR]: Failed to map the file.\n";
			CloseHandle(file_mapping);
			CloseHandle(file_handle);
			return {};
		}

		BaseMeshGeometryData resultData;

		// Determine the number of threads
		const size_t thread_count = importInParallel ? std::thread::hardware_concurrency() : 1;
		const size_t chunk_size = file_size / thread_count;
		std::vector<std::thread> threads(thread_count);
		std::vector<ChunkData> threadResults(thread_count);

		char* file_start = static_cast<char*>(file_memory);
		char* file_end = file_start + file_size;

		for (size_t i = 0; i < thread_count; ++i) {
			char* chunk_start = file_start + (i * chunk_size);
			char* chunk_end = (i == thread_count - 1) ? file_end : chunk_start + chunk_size;

			// Adjust chunk_end to point to the end of a line
			while (*chunk_end != '\n' && chunk_end < file_end) {
				chunk_end++;
			}
			if (chunk_end != file_end) {
				chunk_end++;  // move past the newline character
			}

			// Start a thread to process this chunk
			threads[i] = std::thread(ParseChunk, chunk_start, chunk_end, std::ref(threadResults[i]));
		}

		// Wait for all threads to finish
		for (auto& t : threads) {
			t.join();
		}

		if (chunkIdsVertexPropPtrOpt.has_value() && !chunkIdsVertexPropPtrOpt.value()->empty())
		{
			chunkIdsVertexPropPtrOpt.value()->clear();
		}
		for (int threadId = 0; const auto& result : threadResults)
		{
			if (chunkIdsVertexPropPtrOpt.has_value())
			{
				const auto nVertsPerChunk = result.Vertices.size();
				chunkIdsVertexPropPtrOpt.value()->insert(chunkIdsVertexPropPtrOpt.value()->end(), nVertsPerChunk, static_cast<float>(threadId));
			}
			resultData.Vertices.insert(resultData.Vertices.end(), result.Vertices.begin(), result.Vertices.end());
			resultData.PolyIndices.insert(resultData.PolyIndices.end(), result.PolyIndices.begin(), result.PolyIndices.end());
			resultData.VertexNormals.insert(resultData.VertexNormals.end(), result.VertexNormals.begin(), result.VertexNormals.end());

			++threadId;
		}

		// Clean up
		UnmapViewOfFile(file_memory);
		CloseHandle(file_mapping);
		CloseHandle(file_handle);

		return std::move(resultData);
	}

	std::optional<std::vector<pmp::vec3>> ImportPLYPointCloudData(const std::string& absFileName, const bool& importInParallel)
	{
		const auto extension = Utils::ExtractLowercaseFileExtensionFromPath(absFileName);
		if (extension != "ply")
			return {};

		const char* file_path = absFileName.c_str();

		const HANDLE file_handle = CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file_handle == INVALID_HANDLE_VALUE)
		{
			std::cerr << "ImportPLYPointCloudData [ERROR]: Failed to open the file.\n";
			return {};
		}

		// Get the file size
		const DWORD file_size = GetFileSize(file_handle, nullptr);

		// Create a file mapping object
		const HANDLE file_mapping = CreateFileMapping(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (file_mapping == nullptr)
		{
			std::cerr << "ImportPLYPointCloudData [ERROR]: Failed to create file mapping.\n";
			CloseHandle(file_handle);
			return {};
		}

		// Map the file into memory
		const LPVOID file_memory = MapViewOfFile(file_mapping, FILE_MAP_READ, 0, 0, 0);
		if (file_memory == nullptr)
		{
			std::cerr << "ImportPLYPointCloudData [ERROR]: Failed to map the file.\n";
			CloseHandle(file_mapping);
			CloseHandle(file_handle);
			return {};
		}

		std::vector<pmp::vec3> resultData;

		char* file_start = static_cast<char*>(file_memory);
		char* file_end = file_start + file_size;

		// Read the PLY header to get the number of vertices and start position of vertex data
		const auto [vertexCount, vertexDataStart] = ReadPLYVertexHeader(file_start);
		if (vertexDataStart == 0)
		{
			std::cerr << "ImportPLYPointCloudData [ERROR]: Failed to read PLY header or no vertices found.\n";
			UnmapViewOfFile(file_memory);
			CloseHandle(file_mapping);
			CloseHandle(file_handle);
			return {};
		}

		// Adjust file_start to point to the beginning of vertex data
		file_start = vertexDataStart;

		// Ensure there is vertex data to process
		if (file_start >= file_end) 
		{
			std::cerr << "ImportPLYPointCloudData [ERROR]: No vertex data to process.\n";
			UnmapViewOfFile(file_memory);
			CloseHandle(file_mapping);
			CloseHandle(file_handle);
			return {};
		}

		// Adjust the file size for chunk calculation
		const size_t adjusted_file_size = file_end - file_start;

		// Determine the number of threads and chunk size
		const size_t thread_count = importInParallel ? std::thread::hardware_concurrency() : 1;
		const size_t chunk_size = std::max<size_t>(adjusted_file_size / thread_count, 1ul); // Ensure chunk size is at least 1
		std::vector<std::thread> threads(thread_count);
		std::vector<std::vector<pmp::vec3>> threadResults(thread_count);

		for (size_t i = 0; i < thread_count; ++i) 
		{
			char* chunk_start = file_start + (i * chunk_size);
			char* chunk_end = (i == thread_count - 1) ? file_end : chunk_start + chunk_size;

			// Adjust chunk_start to the beginning of a line (for all chunks except the first)
			if (i > 0) 
			{
				while (*chunk_start != '\n' && chunk_start < file_end) 
				{
					chunk_start++;
				}
				if (chunk_start < file_end)
				{
					chunk_start++;  // Move past the newline character
				}
			}

			// Adjust chunk_end to the end of a line
			while (*chunk_end != '\n' && chunk_end < file_end)
			{
				chunk_end++;
			}
			if (chunk_end < file_end)
			{
				chunk_end++;  // Move past the newline character
			}

			// Start a thread to process this chunk
			threads[i] = std::thread(ParsePointCloudChunk, chunk_start, chunk_end, std::ref(threadResults[i]));
		}

		// Wait for all threads to finish
		for (auto& t : threads) 
		{
			t.join();
		}

		for (const auto& result : threadResults)
		{
			resultData.insert(resultData.end(), result.begin(), result.end());
		}

		// Clean up
		UnmapViewOfFile(file_memory);
		CloseHandle(file_mapping);
		CloseHandle(file_handle);

		return std::move(resultData);
	}

	std::optional<std::vector<pmp::vec3>> ImportPLYPointCloudDataMainThread(const std::string& absFileName)
	{
		const auto extension = Utils::ExtractLowercaseFileExtensionFromPath(absFileName);
		if (extension != "ply")
		{
			std::cerr << absFileName << " has invalid extension!" << std::endl;
			return {};
		}

		std::ifstream file(absFileName);
		if (!file.is_open()) 
		{
			std::cerr << "Failed to open the file." << std::endl;
			return {};
		}

		std::string line;
		size_t vertexCount = 0;
		bool headerEnded = false;

		// Read header to find the vertex count
		while (std::getline(file, line) && !headerEnded) 
		{
			std::istringstream iss(line);
			std::string token;
			iss >> token;

			if (token == "element") {
				iss >> token;
				if (token == "vertex") {
					iss >> vertexCount;
				}
			}
			else if (token == "end_header") {
				headerEnded = true;
			}
		}

		if (!headerEnded || vertexCount == 0) {
			std::cerr << "Invalid PLY header or no vertices found." << std::endl;
			return {};
		}

		std::vector<pmp::vec3> vertices;
		vertices.reserve(vertexCount);

		// Read vertex data
		while (std::getline(file, line)) {
			std::istringstream iss(line);
			pmp::vec3 vertex;
			if (!(iss >> vertex[0] >> vertex[1] >> vertex[2])) {
				std::cerr << "Error parsing vertex data: " << line << std::endl;
				continue;
			}

			vertices.push_back(vertex);
		}

		return vertices;
	}

	bool ExportSampledVerticesToPLY(const BaseMeshGeometryData& meshData, size_t nVerts, const std::string& absFileName, const std::optional<unsigned int>& seed)
	{
		const auto extension = Utils::ExtractLowercaseFileExtensionFromPath(absFileName);
		if (extension != "ply")
		{
			std::cerr << absFileName << " has invalid extension!" << std::endl;
			return false;
		}

		std::ofstream file(absFileName);
		if (!file.is_open())
		{
			std::cerr << "Failed to open file for writing: " << absFileName << std::endl;
			return false;
		}

		// Generate nVerts random indices
		std::vector<size_t> indices;
		std::mt19937 gen;
		if (seed.has_value())
		{
			gen.seed(seed.value());
		}
		else
		{
			std::random_device rd;
			gen = std::mt19937(rd());
		}
		std::uniform_int_distribution<> distrib(0, meshData.Vertices.size() - 1);

		for (size_t i = 0; i < nVerts; ++i)
		{
			indices.push_back(distrib(gen));
		}

		// Write to a .ply file
		std::ofstream outFile(absFileName);
		if (!outFile.is_open()) 
		{
			std::cerr << "Unable to open file: " << absFileName << std::endl;
			return false;
		}

		// PLY header
		outFile << "ply\n";
		outFile << "format ascii 1.0\n";
		outFile << "element vertex " << nVerts << "\n";
		outFile << "property float x\n";
		outFile << "property float y\n";
		outFile << "property float z\n";
		outFile << "end_header\n";

		// Write sampled vertices
		for (auto idx : indices) 
		{
			const auto& vertex = meshData.Vertices[idx];
			outFile << vertex[0] << " " << vertex[1] << " " << vertex[2] << "\n";
		}

		outFile.close();
		return true;
	}

	bool ExportPointsToPLY(const BaseMeshGeometryData& meshData, const std::string& absFileName)
	{
		const auto extension = Utils::ExtractLowercaseFileExtensionFromPath(absFileName);
		if (extension != "ply")
		{
			std::cerr << "Geometry::ExportPointsToPLY:" << absFileName << " has invalid extension!" << std::endl;
			return false;
		}

		if (meshData.Vertices.empty())
		{
			std::cerr << "Geometry::ExportPointsToPLY: No vertices to write!\n";
			return false;
		}

		std::ofstream file(absFileName);
		if (!file.is_open())
		{
			std::cerr << "Geometry::ExportPointsToPLY: Failed to open file for writing: " << absFileName << std::endl;
			return false;
		}

		// Write to a .ply file
		std::ofstream outFile(absFileName);
		if (!outFile.is_open())
		{
			std::cerr << "Geometry::ExportPointsToPLY: Unable to open file: " << absFileName << std::endl;
			return false;
		}

		const unsigned int nVerts = meshData.Vertices.size();

		// PLY header
		outFile << "ply\n";
		outFile << "format ascii 1.0\n";
		outFile << "element vertex " << nVerts << "\n";
		outFile << "property float x\n";
		outFile << "property float y\n";
		outFile << "property float z\n";
		outFile << "end_header\n";

		// Write sampled vertices
		for (const auto& vertex : meshData.Vertices)
		{
			outFile << vertex[0] << " " << vertex[1] << " " << vertex[2] << "\n";
		}

		outFile.close();
		return true;
	}

	bool ExportPolylinesToOBJ(const std::vector<std::vector<pmp::vec3>>& polylines, const std::string& absFileName)
	{
		const auto extension = Utils::ExtractLowercaseFileExtensionFromPath(absFileName);
		if (extension != "obj")
		{
			std::cerr << absFileName << " has invalid extension!" << std::endl;
			return false;
		}

		std::ofstream file(absFileName);
		if (!file.is_open()) 
		{
			std::cerr << "Failed to open file for writing: " << absFileName << std::endl;
			return false;
		}

		// Write vertices
		for (const auto& polyline : polylines)
		{
			for (const auto& vertex : polyline) 
			{
				file << "v " << vertex[0] << " " << vertex[1] << " " << vertex[2] << "\n";
			}
		}

		// Write polyline connections as lines
		size_t indexOffset = 1; // OBJ files are 1-indexed
		for (const auto& polyline : polylines)
		{
			if (polyline.size() < 2) continue; // Ensure there are at least two points to form a segment
			for (size_t i = 0; i < polyline.size() - 1; ++i)
			{
				file << "l " << (i + indexOffset) << " " << (i + indexOffset + 1) << "\n";
			}
			indexOffset += polyline.size();
		}

		file.close();
		return true;
	}

	std::optional<BaseMeshGeometryData> ComputeConvexHullFromPoints(const std::vector<pmp::Point>& points)
	{
		if (points.size() < 4)
		{
			// Not enough points to form a convex hull
			return {};
		}

		using namespace quickhull;
		// convert to quickhull-compatible data
		std::vector<Vector3<float>> qhPtCloud;
		std::ranges::transform(points, std::back_inserter(qhPtCloud),
			[](const pmp::Point& pmpPt) { return Vector3(pmpPt[0], pmpPt[1], pmpPt[2]); });
		QuickHull<float> qh;
		const auto hullResult = qh.getConvexHull(qhPtCloud, true, false);
		const auto& hullVertBuffer = hullResult.getVertexBuffer();

		if (hullVertBuffer.size() < 4)
		{
			// The resulting hull must be at least a tetrahedron
			return {};
		}

		const auto& hullVertIdBuffer = hullResult.getIndexBuffer();
		if (hullVertIdBuffer.size() % 3 != 0)
		{
			// invalid indexing
			return {};
		}

		BaseMeshGeometryData baseMesh;
		std::ranges::transform(hullVertBuffer, std::back_inserter(baseMesh.Vertices),
			[](const auto& qhVert) { return pmp::vec3(qhVert.x, qhVert.y, qhVert.z); });
		for (unsigned int i = 0; i < hullVertIdBuffer.size(); i += 3)
		{
			baseMesh.PolyIndices.push_back({
				static_cast<unsigned int>(hullVertIdBuffer[i]),
				static_cast<unsigned int>(hullVertIdBuffer[i + 1]),
				static_cast<unsigned int>(hullVertIdBuffer[i + 2])
				});
		}
		return baseMesh;
	}
	
	std::optional<pmp::SurfaceMesh> ComputePMPConvexHullFromPoints(const std::vector<pmp::Point>& points)
	{
		const auto baseMeshOpt = ComputeConvexHullFromPoints(points);
		if (!baseMeshOpt.has_value())
			return {};

		return ConvertBufferGeomToPMPSurfaceMesh(baseMeshOpt.value());
	}

	std::pair<pmp::Point, pmp::Scalar> ComputeMeshBoundingSphere(const pmp::SurfaceMesh& mesh)
	{
		if (mesh.is_empty())
		{
			throw std::invalid_argument("Geometry::ComputeMeshBoundingSphere: Vertex positions are not available in the mesh.\n");
		}

		const auto vFirst = mesh.vertices().begin();
		auto vCurrent = vFirst;
		pmp::Point center = mesh.position(*vFirst);
		pmp::Scalar radius = 0.0f;

		// Find the initial point farthest from the arbitrary start point
		for (const auto v : mesh.vertices()) 
		{
			if (norm(mesh.position(v) - center) < radius)
				continue;

			radius = norm(mesh.position(v)- center);
			vCurrent = v;
		}

		// Set sphere center to the midpoint of the initial and farthest point, radius as half the distance
		center = (center + mesh.position(*vCurrent)) * 0.5f;
		radius = norm(mesh.position(*vCurrent) - center);

		// Ensure all points are within the sphere, adjust if necessary
		for (const auto v : mesh.vertices()) 
		{
			const pmp::Scalar dist = norm(mesh.position(v) - center);
			if (dist < radius)
				continue;

			const pmp::Scalar newRadius = (radius + dist) / 2;
			const pmp::Scalar moveBy = newRadius - radius;
			const pmp::Point direction = normalize(mesh.position(v) - center);
			center += direction * moveBy;
			radius = newRadius;
		}

		return { center, radius };
	}

	std::pair<pmp::Point, pmp::Scalar> ComputePointCloudBoundingSphere(const std::vector<pmp::Point>& points)
	{
		if (points.empty())
		{
			throw std::invalid_argument("Geometry::ComputePointCloudBoundingSphere: points.empty()!\n");
		}

		// Start with the first point as the center
		pmp::Point center = points[0];
		pmp::Scalar radius = 0.0f;

		// First pass: find the farthest point from the initial point to set a rough sphere
		for (const auto& point : points)
		{
			const pmp::Scalar dist = norm(point - center);
			if (dist < radius)
				continue;
			radius = dist;
		}

		// Set initial sphere
		center = (center + points[0] + radius * normalize(points[0] - center)) / 2.0;
		radius /= 2.0f;

		// Second pass: expand sphere to include all points
		for (const auto& point : points)
		{
			const pmp::Scalar dist = norm(point - center);
			if (dist < radius) // Point is inside the sphere
				continue;
			const pmp::Scalar newRadius = (radius + dist) / 2;
			const pmp::Scalar moveBy = newRadius - radius;
			const pmp::Point direction = normalize(point - center);
			center += direction * moveBy;
			radius = newRadius;
		}

		return { center, radius };
	}

	//
	// =======================================================================
	// ......... Nanoflann Kd-Tree Utils .....................................
	//

	template <typename T>
	struct PointCloud
	{
		struct Point
		{
			T x, y, z;
		};

		using coord_t = T;  //!< The type of each coordinate

		std::vector<Point> pts;

		// Must return the number of data points
		[[nodiscard]] size_t kdtree_get_point_count() const { return pts.size(); }

		// Returns the dim'th component of the idx'th point in the class:
		// Since this is inlined and the "dim" argument is typically an immediate
		// value, the
		//  "if/else's" are actually solved at compile time.
		[[nodiscard]] T kdtree_get_pt(const size_t idx, const size_t dim) const
		{
			if (dim == 0) return pts[idx].x;
			if (dim == 1) return pts[idx].y;
			return pts[idx].z;
		}

		// Optional bounding-box computation: return false to default to a standard
		// bbox computation loop.
		//   Return true if the BBOX was already computed by the class and returned
		//   in "bb" so it can be avoided to redo it again. Look at bb.size() to
		//   find out the expected dimensionality (e.g. 2 or 3 for point clouds)
		template <class BBOX>
		[[nodiscard]] bool kdtree_get_bbox(BBOX& /* bb */) const
		{
			return false;
		}
	};

	// And this is the "dataset to kd-tree" adaptor class:
	template <typename Derived>
	struct PointCloudAdaptor
	{
		using coord_t = typename Derived::coord_t;

		const Derived& obj;  //!< A const ref to the data set origin

		/// The constructor that sets the data set source
		PointCloudAdaptor(const Derived& obj_) : obj(obj_) {}

		/// CRTP helper method
		[[nodiscard]] const Derived& derived() const { return obj; }

		// Must return the number of data points
		[[nodiscard]] size_t kdtree_get_point_count() const
		{
			return derived().pts.size();
		}

		// Returns the dim'th component of the idx'th point in the class:
		// Since this is inlined and the "dim" argument is typically an immediate
		// value, the
		//  "if/else's" are actually solved at compile time.
		[[nodiscard]] coord_t kdtree_get_pt(const size_t idx, const size_t dim) const
		{
			if (dim == 0) return derived().pts[idx].x;
			if (dim == 1) return derived().pts[idx].y;
			return derived().pts[idx].z;
		}

		// Optional bounding-box computation: return false to default to a standard
		// bbox computation loop.
		//   Return true if the BBOX was already computed by the class and returned
		//   in "bb" so it can be avoided to redo it again. Look at bb.size() to
		//   find out the expected dimensionality (e.g. 2 or 3 for point clouds)
		template <class BBOX>
		bool kdtree_get_bbox(BBOX& /*bb*/) const
		{
			return false;
		}

	};  // end of PointCloudAdaptor

	pmp::Scalar ComputeMinInterVertexDistance(const std::vector<pmp::Point>& points)
	{
		if (points.empty())
		{
			std::cerr << "Geometry::ComputeMinInterVertexDistance: points.empty()!\n";
			return -1.0f;
		}

		// convert points to a compatible dataset
		PointCloud<pmp::Scalar> nfPoints;
		nfPoints.pts.reserve(points.size());
		for (const auto& p : points)
			nfPoints.pts.push_back({ p[0], p[1], p[2] });
		using PointCloudAdapter = PointCloudAdaptor<PointCloud<pmp::Scalar>>;

		// construct a kd-tree index:
		using PointCloudKDTreeIndexAdapter = nanoflann::KDTreeSingleIndexAdaptor<
			nanoflann::L2_Simple_Adaptor<pmp::Scalar, PointCloudAdapter>, PointCloudAdapter, 3 /* dim */>;
		PointCloudKDTreeIndexAdapter indexAdapter(3 /*dim*/, nfPoints, { 10 /* max leaf */ });

		auto do_knn_search = [&indexAdapter](const pmp::Point& p) {
			// do a knn search
			const pmp::Scalar query_pt[3] = { p[0], p[1], p[2] };
			size_t num_results = 2;
			std::vector<uint32_t> ret_index(num_results);
			std::vector<pmp::Scalar> out_dist_sqr(num_results);
			num_results = indexAdapter.knnSearch(
				&query_pt[0], num_results, &ret_index[0], &out_dist_sqr[0]);
			ret_index.resize(num_results);
			out_dist_sqr.resize(num_results);
			return out_dist_sqr[1];
		};

		pmp::Scalar minDistance = std::numeric_limits<pmp::Scalar>::max();
		for (const auto& point : points)
		{
			const auto currDistSq = do_knn_search(point);
			if (currDistSq < minDistance) minDistance = currDistSq;
		}
		return std::sqrt(minDistance);
	}

	pmp::Scalar ComputeNearestNeighborMeanInterVertexDistance(const std::vector<pmp::Point>& points, const size_t& nNeighbors)
	{
		if (points.empty()) 
		{
			std::cerr << "Geometry::ComputeNearestNeighborMeanInterVertexDistance: points.empty()!\n";
			return -1.0f;
		}

		// convert points to a compatible dataset
		PointCloud<pmp::Scalar> nfPoints;
		nfPoints.pts.reserve(points.size());
		for (const auto& p : points)
			nfPoints.pts.push_back({ p[0], p[1], p[2] });
		using PointCloudAdapter = PointCloudAdaptor<PointCloud<pmp::Scalar>>;

		// construct a kd-tree index:
		using PointCloudKDTreeIndexAdapter = nanoflann::KDTreeSingleIndexAdaptor<
			nanoflann::L2_Simple_Adaptor<pmp::Scalar, PointCloudAdapter>, PointCloudAdapter, 3 /* dim */>;
		PointCloudKDTreeIndexAdapter indexAdapter(3 /*dim*/, nfPoints, { 10 /* max leaf */ });

		auto do_knn_search = [&indexAdapter, &nNeighbors](const pmp::Point& p) {
			// do a knn search
			const pmp::Scalar query_pt[3] = { p[0], p[1], p[2] };
			std::vector<uint32_t> ret_index(nNeighbors);
			std::vector<pmp::Scalar> out_dist_sqr(nNeighbors);
			const size_t num_results = indexAdapter.knnSearch(
				&query_pt[0], nNeighbors, &ret_index[0], &out_dist_sqr[0]);
			ret_index.resize(num_results);
			out_dist_sqr.resize(num_results);
			if (num_results == 0) return 0.0f;
			pmp::Scalar totalDistSq = 0.0f;
			for (size_t i = 0; i < num_results; ++i)
				totalDistSq += out_dist_sqr[i];
			return sqrt(totalDistSq / (static_cast<pmp::Scalar>(num_results) - 1.0f));
		};

		pmp::Scalar totalDistance = 0.0f;
		for (const auto& point : points)
		{
			totalDistance += do_knn_search(point);
		}

		return totalDistance / static_cast<pmp::Scalar>(points.size());
	}

	//
	// ===========================================================================================
	//

	pmp::Scalar ComputeMinInterVertexDistanceBruteForce(const std::vector<pmp::Point>& points)
	{
		std::cout << "Geometry::ComputeMinInterVertexDistanceBruteForce [WARNING]: This function is brute-force. Not recommended for large data!\n";
		if (points.size() < 2)
		{
			std::cerr << "Geometry::ComputeMinInterVertexDistanceBruteForce: points.size() < 2! No meaningful distance can be computed!\n";
			return -1.0f;
		}

		pmp::Scalar minDistance = std::numeric_limits<pmp::Scalar>::max();
		for (size_t i = 0; i < points.size(); ++i)
		{
			for (size_t j = i + 1; j < points.size(); ++j) 
			{
				const pmp::Scalar dist = norm(points[i] - points[j]);
				if (dist < minDistance) 
				{
					minDistance = dist;
				}
			}
		}
		return minDistance;
	}

	pmp::Scalar ComputeMaxInterVertexDistanceBruteForce(const std::vector<pmp::Point>& points)
	{
		std::cout << "Geometry::ComputeMaxInterVertexDistanceBruteForce [WARNING]: This function is brute-force. Not recommended for large data!\n";
		if (points.size() < 2)
		{
			std::cerr << "Geometry::ComputeMaxInterVertexDistanceBruteForce: points.size() < 2! No meaningful distance can be computed!\n";
			return -1.0f;
		}

		pmp::Scalar maxDistance = 0;
		for (size_t i = 0; i < points.size(); ++i) 
		{
			for (size_t j = i + 1; j < points.size(); ++j)
			{
				const pmp::Scalar dist = norm(points[i] - points[j]);
				if (dist > maxDistance)
				{
					maxDistance = dist;
				}
			}
		}
		return maxDistance;
	}

	pmp::Scalar ComputeMeanInterVertexDistanceBruteForce(const std::vector<pmp::Point>& points)
	{
		std::cout << "Geometry::ComputeMeanInterVertexDistanceBruteForce [WARNING]: This function is brute-force. Not recommended for large data!\n";
		if (points.size() < 2)
		{
			std::cerr << "Geometry::ComputeMeanInterVertexDistanceBruteForce: points.size() < 2! No meaningful distance can be computed!\n";
			return -1.0f;
		}

		pmp::Scalar totalDistance = 0.0f;
		size_t count = 0;
		for (size_t i = 0; i < points.size(); ++i) 
		{
			for (size_t j = i + 1; j < points.size(); ++j) 
			{
				const pmp::Scalar dist = norm(points[i] - points[j]);
				totalDistance += dist;
				++count;
			}
		}
		return count > 0 ? (totalDistance / count) : 0;
	}

} // namespace Geometry