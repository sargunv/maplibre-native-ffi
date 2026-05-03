const std = @import("std");
const testing = std.testing;
const support = @import("support.zig");
const c = support.c;

test "GeoJSON ABI structs import and compose" {
    const point = c.mln_geometry{
        .size = @sizeOf(c.mln_geometry),
        .type = c.MLN_GEOMETRY_TYPE_POINT,
        .data = .{ .point = .{ .latitude = 37.7749, .longitude = -122.4194 } },
    };
    try testing.expectEqual(@as(u32, c.MLN_GEOMETRY_TYPE_POINT), point.type);

    const name = "name";
    const value = "San Francisco";
    const name_value = c.mln_json_value{
        .size = @sizeOf(c.mln_json_value),
        .type = c.MLN_JSON_VALUE_TYPE_STRING,
        .data = .{ .string_value = .{ .data = value.ptr, .size = value.len } },
    };
    const property = c.mln_json_member{
        .key = .{ .data = name.ptr, .size = name.len },
        .value = &name_value,
    };
    const feature = c.mln_feature{
        .size = @sizeOf(c.mln_feature),
        .geometry = &point,
        .properties = &property,
        .property_count = 1,
        .identifier_type = c.MLN_FEATURE_IDENTIFIER_TYPE_NULL,
        .identifier = .{ .uint_value = 0 },
    };
    const geojson = c.mln_geojson{
        .size = @sizeOf(c.mln_geojson),
        .type = c.MLN_GEOJSON_TYPE_FEATURE,
        .data = .{ .feature = &feature },
    };

    try testing.expectEqual(@as(u32, c.MLN_JSON_VALUE_TYPE_STRING), name_value.type);
    try testing.expectEqual(@as(usize, 1), feature.property_count);
    try testing.expectEqual(@as(u32, c.MLN_GEOJSON_TYPE_FEATURE), geojson.type);
}
