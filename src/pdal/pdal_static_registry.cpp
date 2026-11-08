#include "pdal_static_registry.hpp"

// DuckDB
#include "duckdb/main/extension/extension_loader.hpp"

// PDAL
// We need to create dummy instances of all PDAL plugins in order to force
// linkage when building a static library.
// Filters
// Temporarily disabled: #include <pdal/filters/ApproximateCoplanarFilter.hpp>
#include <pdal/filters/AssignFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ChipperFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ClusterFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ColorinterpFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ColorizationFilter.hpp>
// Temporarily disabled: #include <pdal/filters/CovarianceFeaturesFilter.hpp>
// Temporarily disabled: #include <pdal/filters/CropFilter.hpp>
// Temporarily disabled: #include <pdal/filters/CSFilter.hpp>
// Temporarily disabled: #include <pdal/filters/DBSCANFilter.hpp>
// Temporarily disabled: #include <pdal/filters/DecimationFilter.hpp>
// Temporarily disabled: #include <pdal/filters/DelaunayFilter.hpp>
// Temporarily disabled: #include <pdal/filters/DEMFilter.hpp>
// Temporarily disabled: #include <pdal/filters/DividerFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ELMFilter.hpp>
// Temporarily disabled: #include <pdal/filters/EstimateRankFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ExpressionFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ExpressionStatsFilter.hpp>
#include <pdal/filters/FaceRasterFilter.hpp>
// Temporarily disabled: #include <pdal/filters/FarthestPointSamplingFilter.hpp>
#include <pdal/filters/FerryFilter.hpp>
// Temporarily disabled: #include <pdal/filters/GeomDistanceFilter.hpp>
// Temporarily disabled: #include <pdal/filters/GeoreferenceFilter.hpp>
// Temporarily disabled: #include <pdal/filters/GridDecimationFilter.hpp>
// Temporarily disabled: #include <pdal/filters/GroupByFilter.hpp>
#include <pdal/filters/HagDelaunayFilter.hpp>
// Temporarily disabled: #include <pdal/filters/HagDemFilter.hpp>
// Temporarily disabled: #include <pdal/filters/HagNnFilter.hpp>
// Temporarily disabled: #include <pdal/filters/HeadFilter.hpp>
// Temporarily disabled: #include <pdal/filters/HexBinFilter.hpp>
// Temporarily disabled: #include <pdal/filters/InfoFilter.hpp>
// Temporarily disabled: #include <pdal/filters/IQRFilter.hpp>
// Temporarily disabled: #include <pdal/filters/LabelDuplicatesFilter.hpp>
// Temporarily disabled: #include <pdal/filters/LiTreeFilter.hpp>
// Temporarily disabled: #include <pdal/filters/LloydKMeansFilter.hpp>
// Temporarily disabled: #include <pdal/filters/LocateFilter.hpp>
// Temporarily disabled: #include <pdal/filters/LOFFilter.hpp>
// Temporarily disabled: #include <pdal/filters/M3C2Filter.hpp>
// Temporarily disabled: #include <pdal/filters/MADFilter.hpp>
// Temporarily disabled: #include <pdal/filters/MergeFilter.hpp>
// Temporarily disabled: #include <pdal/filters/MiniballFilter.hpp>
// Temporarily disabled: #include <pdal/filters/MongoExpressionFilter.hpp>
// Temporarily disabled: #include <pdal/filters/MortonOrderFilter.hpp>
// Temporarily disabled: #include <pdal/filters/NeighborClassifierFilter.hpp>
// Temporarily disabled: #include <pdal/filters/NNDistanceFilter.hpp>
// Temporarily disabled: #include <pdal/filters/NormalFilter.hpp>
// Temporarily disabled: #include <pdal/filters/OptimalNeighborhoodFilter.hpp>
// Temporarily disabled: #include <pdal/filters/OutlierFilter.hpp>
#include <pdal/filters/OverlayFilter.hpp>
// Temporarily disabled: #include <pdal/filters/PlaneFitFilter.hpp>
// Temporarily disabled: #include <pdal/filters/PMFFilter.hpp>
// Temporarily disabled: #include <pdal/filters/PoissonFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ProjPipelineFilter.hpp>
// Temporarily disabled: #include <pdal/filters/RadialDensityFilter.hpp>
// Temporarily disabled: #include <pdal/filters/RadiusAssignFilter.hpp>
// Temporarily disabled: #include <pdal/filters/RandomizeFilter.hpp>
// Temporarily disabled: #include <pdal/filters/RangeFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ReciprocityFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ReprojectionFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ReturnsFilter.hpp>
// Temporarily disabled: #include <pdal/filters/SampleFilter.hpp>
// Temporarily disabled: #include <pdal/filters/SeparateScanLineFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ShellFilter.hpp>
// Temporarily disabled: #include <pdal/filters/SkewnessBalancingFilter.hpp>
// Temporarily disabled: #include <pdal/filters/SMRFilter.hpp>
// Temporarily disabled: #include <pdal/filters/SortFilter.hpp>
// Temporarily disabled: #include <pdal/filters/SparseSurfaceFilter.hpp>
// Temporarily disabled: #include <pdal/filters/SplitterFilter.hpp>
// Temporarily disabled: #include <pdal/filters/StatsFilter.hpp>
// Temporarily disabled: #include <pdal/filters/StreamCallbackFilter.hpp>
// Temporarily disabled: #include <pdal/filters/SupervoxelFilter.hpp>
#include <pdal/filters/TailFilter.hpp>
// Temporarily disabled: #include <pdal/filters/TransformationFilter.hpp>
// Temporarily disabled: #include <pdal/filters/VoxelCenterNearestNeighborFilter.hpp>
// Temporarily disabled: #include <pdal/filters/VoxelCentroidNearestNeighborFilter.hpp>
// Temporarily disabled: #include <pdal/filters/VoxelDownsizeFilter.hpp>
// Temporarily disabled: #include <pdal/filters/ZsmoothFilter.hpp>
// Private types and utilities
#include <pdal/filters/private/DimRange.hpp>
#include <pdal/private/gdal/Raster.hpp>
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
#include <pdal/io/StacReader.hpp>
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

			// Temporarily disabled: volatile auto *approximate_coplanar_filter = new pdal::ApproximateCoplanarFilter();
			// Temporarily disabled: delete approximate_coplanar_filter;
			volatile auto *assign_filter = new pdal::AssignFilter();
			delete assign_filter;

			// Temporarily disabled: volatile auto *chipper_filter = new pdal::ChipperFilter();
			// Temporarily disabled: delete chipper_filter;
			// Temporarily disabled: volatile auto *cluster_filter = new pdal::ClusterFilter();
			// Temporarily disabled: delete cluster_filter;
			// Temporarily disabled: volatile auto *colorinterp_filter = new pdal::ColorinterpFilter();
			// Temporarily disabled: delete colorinterp_filter;
			// Temporarily disabled: volatile auto *colorization_filter = new pdal::ColorizationFilter();
			// Temporarily disabled: delete colorization_filter;
			// Temporarily disabled: volatile auto *covariance_features_filter = new pdal::CovarianceFeaturesFilter();
			// Temporarily disabled: delete covariance_features_filter;
			// Temporarily disabled: volatile auto *crop_filter = new pdal::CropFilter();
			// Temporarily disabled: delete crop_filter;
			// Temporarily disabled: volatile auto *cs_filter = new pdal::CSFilter();
			// Temporarily disabled: delete cs_filter;
			// Temporarily disabled: volatile auto *dbscan_filter = new pdal::DBSCANFilter();
			// Temporarily disabled: delete dbscan_filter;
			// Temporarily disabled: volatile auto *decimation_filter = new pdal::DecimationFilter();
			// Temporarily disabled: delete decimation_filter;
			// Temporarily disabled: volatile auto *delaunay_filter = new pdal::DelaunayFilter();
			// Temporarily disabled: delete delaunay_filter;
			// Temporarily disabled: volatile auto *dem_filter = new pdal::DEMFilter();
			// Temporarily disabled: delete dem_filter;
			// Temporarily disabled: volatile auto *divider_filter = new pdal::DividerFilter();
			// Temporarily disabled: delete divider_filter;
			// Temporarily disabled: volatile auto *elm_filter = new pdal::ELMFilter();
			// Temporarily disabled: delete elm_filter;
			// Temporarily disabled: volatile auto *estimate_rank_filter = new pdal::EstimateRankFilter();
			// Temporarily disabled: delete estimate_rank_filter;
			// Temporarily disabled: volatile auto *expression_filter = new pdal::ExpressionFilter();
			// Temporarily disabled: delete expression_filter;
			// Temporarily disabled: volatile auto *expression_stats_filter = new pdal::ExpressionStatsFilter();
			// Temporarily disabled: delete expression_stats_filter;
			volatile auto *face_raster_filter = new pdal::FaceRasterFilter();
			delete face_raster_filter;

			// Temporarily disabled: volatile auto *farthest_point_sampling_filter = new pdal::FarthestPointSamplingFilter();
			// Temporarily disabled: delete farthest_point_sampling_filter;
			volatile auto *ferry_filter = new pdal::FerryFilter();
			delete ferry_filter;

			// Temporarily disabled: volatile auto *geom_distance_filter = new pdal::GeomDistanceFilter();
			// Temporarily disabled: delete geom_distance_filter;
			// Temporarily disabled: volatile auto *georeference_filter = new pdal::GeoreferenceFilter();
			// Temporarily disabled: delete georeference_filter;
			// Temporarily disabled: volatile auto *grid_decimation_filter = new pdal::GridDecimationFilter();
			// Temporarily disabled: delete grid_decimation_filter;
			// Temporarily disabled: volatile auto *group_by_filter = new pdal::GroupByFilter();
			// Temporarily disabled: delete group_by_filter;
			volatile auto *hag_delaunay_filter = new pdal::HagDelaunayFilter();
			delete hag_delaunay_filter;

			// Temporarily disabled: volatile auto *hag_dem_filter = new pdal::HagDemFilter();
			// Temporarily disabled: delete hag_dem_filter;
			// Temporarily disabled: volatile auto *hag_nn_filter = new pdal::HagNnFilter();
			// Temporarily disabled: delete hag_nn_filter;
			// Temporarily disabled: volatile auto *head_filter = new pdal::HeadFilter();
			// Temporarily disabled: delete head_filter;
			// Temporarily disabled: volatile auto *hex_bin_filter = new pdal::HexBin();
			// Temporarily disabled: delete hex_bin_filter;
			// Temporarily disabled: volatile auto *info_filter = new pdal::InfoFilter();
			// Temporarily disabled: delete info_filter;
			// Temporarily disabled: volatile auto *iqr_filter = new pdal::IQRFilter();
			// Temporarily disabled: delete iqr_filter;
			// Temporarily disabled: volatile auto *label_duplicates_filter = new pdal::LabelDuplicatesFilter();
			// Temporarily disabled: delete label_duplicates_filter;
			// Temporarily disabled: volatile auto *li_tree_filter = new pdal::LiTreeFilter();
			// Temporarily disabled: delete li_tree_filter;
			// Temporarily disabled: volatile auto *lloyd_kmeans_filter = new pdal::LloydKMeansFilter();
			// Temporarily disabled: delete lloyd_kmeans_filter;
			// Temporarily disabled: volatile auto *locate_filter = new pdal::LocateFilter();
			// Temporarily disabled: delete locate_filter;
			// Temporarily disabled: volatile auto *lof_filter = new pdal::LOFFilter();
			// Temporarily disabled: delete lof_filter;
			// Temporarily disabled: volatile auto *m3c2_filter = new pdal::M3C2Filter();
			// Temporarily disabled: delete m3c2_filter;
			// Temporarily disabled: volatile auto *mad_filter = new pdal::MADFilter();
			// Temporarily disabled: delete mad_filter;
			// Temporarily disabled: volatile auto *merge_filter = new pdal::MergeFilter();
			// Temporarily disabled: delete merge_filter;
			// Temporarily disabled: volatile auto *miniball_filter = new pdal::MiniballFilter();
			// Temporarily disabled: delete miniball_filter;
			// Temporarily disabled: volatile auto *mongo_expression_filter = new pdal::MongoExpressionFilter();
			// Temporarily disabled: delete mongo_expression_filter;
			// Temporarily disabled: volatile auto *morton_order_filter = new pdal::MortonOrderFilter();
			// Temporarily disabled: delete morton_order_filter;
			// Temporarily disabled: volatile auto *neighbor_classifier_filter = new pdal::NeighborClassifierFilter();
			// Temporarily disabled: delete neighbor_classifier_filter;
			// Temporarily disabled: volatile auto *nn_distance_filter = new pdal::NNDistanceFilter();
			// Temporarily disabled: delete nn_distance_filter;
			// Temporarily disabled: volatile auto *normal_filter = new pdal::NormalFilter();
			// Temporarily disabled: delete normal_filter;
			// Temporarily disabled: volatile auto *optimal_neighborhood_filter = new pdal::OptimalNeighborhood();
			// Temporarily disabled: delete optimal_neighborhood_filter;
			// Temporarily disabled: volatile auto *outlier_filter = new pdal::OutlierFilter();
			// Temporarily disabled: delete outlier_filter;
			volatile auto *overlay_filter = new pdal::OverlayFilter();
			delete overlay_filter;

			// Temporarily disabled: volatile auto *plane_fit_filter = new pdal::PlaneFitFilter();
			// Temporarily disabled: delete plane_fit_filter;
			// Temporarily disabled: volatile auto *pmf_filter = new pdal::PMFFilter();
			// Temporarily disabled: delete pmf_filter;
			// Temporarily disabled: volatile auto *poisson_filter = new pdal::PoissonFilter();
			// Temporarily disabled: delete poisson_filter;
			// Temporarily disabled: volatile auto *proj_pipeline_filter = new pdal::ProjPipelineFilter();
			// Temporarily disabled: delete proj_pipeline_filter;
			// Temporarily disabled: volatile auto *radial_density_filter = new pdal::RadialDensityFilter();
			// Temporarily disabled: delete radial_density_filter;
			// Temporarily disabled: volatile auto *radius_assign_filter = new pdal::RadiusAssignFilter();
			// Temporarily disabled: delete radius_assign_filter;
			// Temporarily disabled: volatile auto *randomize_filter = new pdal::RandomizeFilter();
			// Temporarily disabled: delete randomize_filter;
			// Temporarily disabled: volatile auto *range_filter = new pdal::RangeFilter();
			// Temporarily disabled: delete range_filter;
			// Temporarily disabled: volatile auto *reciprocity_filter = new pdal::ReciprocityFilter();
			// Temporarily disabled: delete reciprocity_filter;
			// Temporarily disabled: volatile auto *reprojection_filter = new pdal::ReprojectionFilter();
			// Temporarily disabled: delete reprojection_filter;
			// Temporarily disabled: volatile auto *returns_filter = new pdal::ReturnsFilter();
			// Temporarily disabled: delete returns_filter;
			// Temporarily disabled: volatile auto *sample_filter = new pdal::SampleFilter();
			// Temporarily disabled: delete sample_filter;
			// Temporarily disabled: volatile auto *separate_scan_line_filter = new pdal::SeparateScanLineFilter();
			// Temporarily disabled: delete separate_scan_line_filter;
			// Temporarily disabled: volatile auto *shell_filter = new pdal::ShellFilter();
			// Temporarily disabled: delete shell_filter;
			// Temporarily disabled: volatile auto *skewness_balancing_filter = new pdal::SkewnessBalancingFilter();
			// Temporarily disabled: delete skewness_balancing_filter;
			// Temporarily disabled: volatile auto *smr_filter = new pdal::SMRFilter();
			// Temporarily disabled: delete smr_filter;
			// Temporarily disabled: volatile auto *sort_filter = new pdal::SortFilter();
			// Temporarily disabled: delete sort_filter;
			// Temporarily disabled: volatile auto *sparse_surface_filter = new pdal::SparseSurfaceFilter();
			// Temporarily disabled: delete sparse_surface_filter;
			// Temporarily disabled: volatile auto *splitter_filter = new pdal::SplitterFilter();
			// Temporarily disabled: delete splitter_filter;
			// Temporarily disabled: volatile auto *stats_filter = new pdal::StatsFilter();
			// Temporarily disabled: delete stats_filter;
			// Temporarily disabled: volatile auto *stream_callback_filter = new pdal::StreamCallbackFilter();
			// Temporarily disabled: delete stream_callback_filter;
			// Temporarily disabled: volatile auto *supervoxel_filter = new pdal::SupervoxelFilter();
			// Temporarily disabled: delete supervoxel_filter;
			volatile auto *tail_filter = new pdal::TailFilter();
			delete tail_filter;

			// Temporarily disabled: volatile auto *transformation_filter = new pdal::TransformationFilter();
			// Temporarily disabled: delete transformation_filter;
			// Temporarily disabled: volatile auto *voxel_center_nn_filter = new pdal::VoxelCenterNearestNeighborFilter();
			// Temporarily disabled: delete voxel_center_nn_filter;
			// Temporarily disabled: volatile auto *voxel_centroid_nn_filter = new pdal::VoxelCentroidNearestNeighborFilter();
			// Temporarily disabled: delete voxel_centroid_nn_filter;
			// Temporarily disabled: volatile auto *voxel_downsize_filter = new pdal::VoxelDownsizeFilter();
			// Temporarily disabled: delete voxel_downsize_filter;
			// Temporarily disabled: volatile auto *zsmooth_filter = new pdal::ZsmoothFilter();
			// Temporarily disabled: delete zsmooth_filter;
			// ==========================================================================================================
			// Readers & Writers
			// ==========================================================================================================

			// Temporarily disabled: volatile auto *bpf_reader = new pdal::BpfReader();
			// Temporarily disabled: delete bpf_reader;
			// Temporarily disabled: volatile auto *bpf_writer = new pdal::BpfWriter();
			// Temporarily disabled: delete bpf_writer;
			// Temporarily disabled: volatile auto *buffer_reader = new pdal::BufferReader();
			// Temporarily disabled: delete buffer_reader;
			// Temporarily disabled: volatile auto *copc_reader = new pdal::CopcReader();
			// Temporarily disabled: delete copc_reader;
			// Temporarily disabled: volatile auto *copc_writer = new pdal::CopcWriter();
			// Temporarily disabled: delete copc_writer;
			// Temporarily disabled: volatile auto *ept_addon_writer = new pdal::EptAddonWriter();
			// Temporarily disabled: delete ept_addon_writer;
			// Temporarily disabled: volatile auto *ept_reader = new pdal::EptReader();
			// Temporarily disabled: delete ept_reader;
			// Temporarily disabled: volatile auto *faux_reader = new pdal::FauxReader();
			// Temporarily disabled: delete faux_reader;
			// Temporarily disabled: volatile auto *fbi_reader = new pdal::FbiReader();
			// Temporarily disabled: delete fbi_reader;
			// Temporarily disabled: volatile auto *fbi_writer = new pdal::FbiWriter();
			// Temporarily disabled: delete fbi_writer;
			// Temporarily disabled: volatile auto *gdal_reader = new pdal::GDALReader();
			// Temporarily disabled: delete gdal_reader;
			// Temporarily disabled: volatile auto *gdal_writer = new pdal::GDALWriter();
			// Temporarily disabled: delete gdal_writer;
			// Temporarily disabled: volatile auto *gltf_writer = new pdal::GltfWriter();
			// Temporarily disabled: delete gltf_writer;
			// Temporarily disabled: volatile auto *las_reader = new pdal::LasReader();
			// Temporarily disabled: delete las_reader;
			// Temporarily disabled: volatile auto *las_writer = new pdal::LasWriter();
			// Temporarily disabled: delete las_writer;
			// Temporarily disabled: volatile auto *memory_view_reader = new pdal::MemoryViewReader();
			// Temporarily disabled: delete memory_view_reader;
			// Temporarily disabled: volatile auto *null_writer = new pdal::NullWriter();
			// Temporarily disabled: delete null_writer;
			// Temporarily disabled: volatile auto *obj_reader = new pdal::ObjReader();
			// Temporarily disabled: delete obj_reader;
			// Temporarily disabled: volatile auto *ogr_writer = new pdal::OGRWriter();
			// Temporarily disabled: delete ogr_writer;
			// Temporarily disabled: volatile auto *optech_reader = new pdal::OptechReader();
			// Temporarily disabled: delete optech_reader;
			// Temporarily disabled: volatile auto *pcd_reader = new pdal::PcdReader();
			// Temporarily disabled: delete pcd_reader;
			// Temporarily disabled: volatile auto *pcd_writer = new pdal::PcdWriter();
			// Temporarily disabled: delete pcd_writer;
			// Temporarily disabled: volatile auto *ply_reader = new pdal::PlyReader();
			// Temporarily disabled: delete ply_reader;
			// Temporarily disabled: volatile auto *ply_writer = new pdal::PlyWriter();
			// Temporarily disabled: delete ply_writer;
			// Temporarily disabled: volatile auto *pts_reader = new pdal::PtsReader();
			// Temporarily disabled: delete pts_reader;
			// Temporarily disabled: volatile auto *ptx_reader = new pdal::PtxReader();
			// Temporarily disabled: delete ptx_reader;
			// Temporarily disabled: volatile auto *qfit_reader = new pdal::QfitReader();
			// Temporarily disabled: delete qfit_reader;
			// Temporarily disabled: volatile auto *raster_writer = new pdal::RasterWriter();
			// Temporarily disabled: delete raster_writer;
			// Temporarily disabled: volatile auto *sbet_reader = new pdal::SbetReader();
			// Temporarily disabled: delete sbet_reader;
			// Temporarily disabled: volatile auto *sbet_writer = new pdal::SbetWriter();
			// Temporarily disabled: delete sbet_writer;
			// Temporarily disabled: volatile auto *stac_reader = new pdal::StacReader();
			// Temporarily disabled: delete stac_reader;
			// Temporarily disabled: volatile auto *terrasolid_reader = new pdal::TerrasolidReader();
			// Temporarily disabled: delete terrasolid_reader;
			// Temporarily disabled: volatile auto *text_reader = new pdal::TextReader();
			// Temporarily disabled: delete text_reader;
			// Temporarily disabled: volatile auto *text_writer = new pdal::TextWriter();
			// Temporarily disabled: delete text_writer;
		} catch (Exception &error) {
			std::cout << "PDAL Static Registry error: " << error.what() << std::endl;
		}
	});
}

} // namespace duckdb
