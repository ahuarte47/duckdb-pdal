#include "pdal_static_registry.hpp"

// DuckDB
#include "duckdb/main/extension/extension_loader.hpp"

// PDAL
// We need to create dummy instances of all PDAL plugins in order to force
// linkage when building a static library.
// Filters
#include <pdal/filters/ApproximateCoplanarFilter.hpp>
#include <pdal/filters/AssignFilter.hpp>
#include <pdal/filters/ChipperFilter.hpp>
#include <pdal/filters/ClusterFilter.hpp>
#include <pdal/filters/ColorinterpFilter.hpp>
#include <pdal/filters/ColorizationFilter.hpp>
#include <pdal/filters/CovarianceFeaturesFilter.hpp>
#include <pdal/filters/CropFilter.hpp>
#include <pdal/filters/CSFilter.hpp>
#include <pdal/filters/DBSCANFilter.hpp>
#include <pdal/filters/DecimationFilter.hpp>
#include <pdal/filters/DelaunayFilter.hpp>
#include <pdal/filters/DividerFilter.hpp>
#include <pdal/filters/ELMFilter.hpp>
#include <pdal/filters/EstimateRankFilter.hpp>
#include <pdal/filters/ExpressionFilter.hpp>
#include <pdal/filters/FerryFilter.hpp>
#include <pdal/filters/HeadFilter.hpp>
#include <pdal/filters/InfoFilter.hpp>
#include <pdal/filters/IQRFilter.hpp>
#include <pdal/filters/LocateFilter.hpp>
#include <pdal/filters/MADFilter.hpp>
#include <pdal/filters/MergeFilter.hpp>
#include <pdal/filters/MortonOrderFilter.hpp>
#include <pdal/filters/NormalFilter.hpp>
#include <pdal/filters/OutlierFilter.hpp>
#include <pdal/filters/OverlayFilter.hpp>
#include <pdal/filters/PMFFilter.hpp>
#include <pdal/filters/RandomizeFilter.hpp>
#include <pdal/filters/RangeFilter.hpp>
#include <pdal/filters/ReciprocityFilter.hpp>
#include <pdal/filters/ReprojectionFilter.hpp>
#include <pdal/filters/ReturnsFilter.hpp>
#include <pdal/filters/SampleFilter.hpp>
#include <pdal/filters/ShellFilter.hpp>
#include <pdal/filters/SkewnessBalancingFilter.hpp>
#include <pdal/filters/SMRFilter.hpp>
#include <pdal/filters/SortFilter.hpp>
#include <pdal/filters/SplitterFilter.hpp>
#include <pdal/filters/StatsFilter.hpp>
#include <pdal/filters/StreamCallbackFilter.hpp>
#include <pdal/filters/TailFilter.hpp>
#include <pdal/filters/TransformationFilter.hpp>
#include <pdal/filters/VoxelCenterNearestNeighborFilter.hpp>
#include <pdal/filters/VoxelCentroidNearestNeighborFilter.hpp>
#include <pdal/filters/ZsmoothFilter.hpp>
// Readers/Writers
#include <pdal/io/BpfReader.hpp>
#include <pdal/io/BpfWriter.hpp>
#include <pdal/io/BufferReader.hpp>
#include <pdal/io/CopcReader.hpp>
#include <pdal/io/CopcWriter.hpp>
#include <pdal/io/EptAddonWriter.hpp>
#include <pdal/io/EptReader.hpp>
#include <pdal/io/FauxReader.hpp>
#include <pdal/io/FbiReader.hpp>
#include <pdal/io/FbiWriter.hpp>
#include <pdal/io/private/GDALGrid.hpp>
#include <pdal/io/GDALReader.hpp>
#include <pdal/io/GDALWriter.hpp>
#include <pdal/io/GltfWriter.hpp>
#include <pdal/io/LasReader.hpp>
#include <pdal/io/LasWriter.hpp>
#include <pdal/io/MemoryViewReader.hpp>
#include <pdal/io/NullWriter.hpp>
#include <pdal/io/ObjReader.hpp>
#include <pdal/io/OGRWriter.hpp>
#include <pdal/io/OptechReader.hpp>
#include <pdal/io/PcdReader.hpp>
#include <pdal/io/PcdWriter.hpp>
#include <pdal/io/PlyReader.hpp>
#include <pdal/io/PlyWriter.hpp>
#include <pdal/io/PtsReader.hpp>
#include <pdal/io/PtxReader.hpp>
#include <pdal/io/QfitReader.hpp>
#include <pdal/io/RasterWriter.hpp>
#include <pdal/io/SbetReader.hpp>
#include <pdal/io/SbetWriter.hpp>
#include <pdal/io/TerrasolidReader.hpp>
#include <pdal/io/TextReader.hpp>
#include <pdal/io/TextWriter.hpp>

namespace duckdb {

// ######################################################################################################################
// PDAL Static Registry
// ######################################################################################################################

void PdalStaticRegistry::Register(ExtensionLoader &loader) {

	// Create & delete dummy instances to force linkage (once)
	static std::once_flag loaded;
	std::call_once(loaded, [&]() {
		try {
			// ==========================================================================================================
			// Filters
			// ==========================================================================================================

			volatile auto *approximate_coplanar_filter = new pdal::ApproximateCoplanarFilter();
			delete approximate_coplanar_filter;

			volatile auto *assign_filter = new pdal::AssignFilter();
			delete assign_filter;

			volatile auto *chipper_filter = new pdal::ChipperFilter();
			delete chipper_filter;

			volatile auto *cluster_filter = new pdal::ClusterFilter();
			delete cluster_filter;

			volatile auto *colorinterp_filter = new pdal::ColorinterpFilter();
			delete colorinterp_filter;

			volatile auto *colorization_filter = new pdal::ColorizationFilter();
			delete colorization_filter;

			volatile auto *covariance_features_filter = new pdal::CovarianceFeaturesFilter();
			delete covariance_features_filter;

			volatile auto *crop_filter = new pdal::CropFilter();
			delete crop_filter;

			volatile auto *cs_filter = new pdal::CSFilter();
			delete cs_filter;

			volatile auto *dbscan_filter = new pdal::DBSCANFilter();
			delete dbscan_filter;

			volatile auto *decimation_filter = new pdal::DecimationFilter();
			delete decimation_filter;

			volatile auto *delaunay_filter = new pdal::DelaunayFilter();
			delete delaunay_filter;

			volatile auto *divider_filter = new pdal::DividerFilter();
			delete divider_filter;

			volatile auto *elm_filter = new pdal::ELMFilter();
			delete elm_filter;

			volatile auto *estimate_rank_filter = new pdal::EstimateRankFilter();
			delete estimate_rank_filter;

			volatile auto *expression_filter = new pdal::ExpressionFilter();
			delete expression_filter;

			volatile auto *ferry_filter = new pdal::FerryFilter();
			delete ferry_filter;

			volatile auto *head_filter = new pdal::HeadFilter();
			delete head_filter;

			volatile auto *info_filter = new pdal::InfoFilter();
			delete info_filter;

			volatile auto *iqr_filter = new pdal::IQRFilter();
			delete iqr_filter;

			volatile auto *locate_filter = new pdal::LocateFilter();
			delete locate_filter;

			volatile auto *mad_filter = new pdal::MADFilter();
			delete mad_filter;

			volatile auto *merge_filter = new pdal::MergeFilter();
			delete merge_filter;

			volatile auto *morton_order_filter = new pdal::MortonOrderFilter();
			delete morton_order_filter;

			volatile auto *normal_filter = new pdal::NormalFilter();
			delete normal_filter;

			volatile auto *outlier_filter = new pdal::OutlierFilter();
			delete outlier_filter;

			volatile auto *overlay_filter = new pdal::OverlayFilter();
			delete overlay_filter;

			volatile auto *pmf_filter = new pdal::PMFFilter();
			delete pmf_filter;

			volatile auto *randomize_filter = new pdal::RandomizeFilter();
			delete randomize_filter;

			volatile auto *range_filter = new pdal::RangeFilter();
			delete range_filter;

			volatile auto *reciprocity_filter = new pdal::ReciprocityFilter();
			delete reciprocity_filter;

			volatile auto *reprojection_filter = new pdal::ReprojectionFilter();
			delete reprojection_filter;

			volatile auto *returns_filter = new pdal::ReturnsFilter();
			delete returns_filter;

			volatile auto *sample_filter = new pdal::SampleFilter();
			delete sample_filter;

			volatile auto *shell_filter = new pdal::ShellFilter();
			delete shell_filter;

			volatile auto *skewness_balancing_filter = new pdal::SkewnessBalancingFilter();
			delete skewness_balancing_filter;

			volatile auto *smr_filter = new pdal::SMRFilter();
			delete smr_filter;

			volatile auto *sort_filter = new pdal::SortFilter();
			delete sort_filter;

			volatile auto *splitter_filter = new pdal::SplitterFilter();
			delete splitter_filter;

			volatile auto *stats_filter = new pdal::StatsFilter();
			delete stats_filter;

			volatile auto *stream_callback_filter = new pdal::StreamCallbackFilter();
			delete stream_callback_filter;

			volatile auto *tail_filter = new pdal::TailFilter();
			delete tail_filter;

			volatile auto *transformation_filter = new pdal::TransformationFilter();
			delete transformation_filter;

			volatile auto *voxel_center_nn_filter = new pdal::VoxelCenterNearestNeighborFilter();
			delete voxel_center_nn_filter;

			volatile auto *voxel_centroid_nn_filter = new pdal::VoxelCentroidNearestNeighborFilter();
			delete voxel_centroid_nn_filter;

			volatile auto *zsmooth_filter = new pdal::ZsmoothFilter();
			delete zsmooth_filter;

			// ==========================================================================================================
			// Readers & Writers
			// ==========================================================================================================

			volatile auto *bpf_reader = new pdal::BpfReader();
			delete bpf_reader;

			volatile auto *bpf_writer = new pdal::BpfWriter();
			delete bpf_writer;

			volatile auto *buffer_reader = new pdal::BufferReader();
			delete buffer_reader;

			volatile auto *copc_reader = new pdal::CopcReader();
			delete copc_reader;

			volatile auto *copc_writer = new pdal::CopcWriter();
			delete copc_writer;

			volatile auto *ept_addon_writer = new pdal::EptAddonWriter();
			delete ept_addon_writer;

			volatile auto *ept_reader = new pdal::EptReader();
			delete ept_reader;

			volatile auto *faux_reader = new pdal::FauxReader();
			delete faux_reader;

			volatile auto *fbi_reader = new pdal::FbiReader();
			delete fbi_reader;

			volatile auto *fbi_writer = new pdal::FbiWriter();
			delete fbi_writer;

			volatile auto *gdal_reader = new pdal::GDALReader();
			delete gdal_reader;

			volatile auto *gdal_writer = new pdal::GDALWriter();
			delete gdal_writer;

			volatile auto *gltf_writer = new pdal::GltfWriter();
			delete gltf_writer;

			volatile auto *las_reader = new pdal::LasReader();
			delete las_reader;

			volatile auto *las_writer = new pdal::LasWriter();
			delete las_writer;

			volatile auto *memory_view_reader = new pdal::MemoryViewReader();
			delete memory_view_reader;

			volatile auto *null_writer = new pdal::NullWriter();
			delete null_writer;

			volatile auto *obj_reader = new pdal::ObjReader();
			delete obj_reader;

			volatile auto *ogr_writer = new pdal::OGRWriter();
			delete ogr_writer;

			volatile auto *optech_reader = new pdal::OptechReader();
			delete optech_reader;

			volatile auto *pcd_reader = new pdal::PcdReader();
			delete pcd_reader;

			volatile auto *pcd_writer = new pdal::PcdWriter();
			delete pcd_writer;

			volatile auto *ply_reader = new pdal::PlyReader();
			delete ply_reader;

			volatile auto *ply_writer = new pdal::PlyWriter();
			delete ply_writer;

			volatile auto *pts_reader = new pdal::PtsReader();
			delete pts_reader;

			volatile auto *ptx_reader = new pdal::PtxReader();
			delete ptx_reader;

			volatile auto *qfit_reader = new pdal::QfitReader();
			delete qfit_reader;

			volatile auto *raster_writer = new pdal::RasterWriter();
			delete raster_writer;

			volatile auto *sbet_reader = new pdal::SbetReader();
			delete sbet_reader;

			volatile auto *sbet_writer = new pdal::SbetWriter();
			delete sbet_writer;

			volatile auto *terrasolid_reader = new pdal::TerrasolidReader();
			delete terrasolid_reader;

			volatile auto *text_reader = new pdal::TextReader();
			delete text_reader;

			volatile auto *text_writer = new pdal::TextWriter();
			delete text_writer;

		} catch (Exception &error) {
			std::cout << "PDAL Static Registry error: " << error.what() << std::endl;
		}
	});
}

} // namespace duckdb
