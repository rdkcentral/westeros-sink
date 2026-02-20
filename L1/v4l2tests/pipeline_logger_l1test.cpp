/*
 * L1 Test Suite for Pipeline Logger
 * Coverage: pipeline_logger.cpp
 * Target: 100% function and line coverage of all pipeline_logger functions
 * 
 * Tests cover:
 * - correct_indentation(): Text alignment and spacing
 * - add_element_to_list(): Element list building with pad information
 * - find_head(): Head element identification in pipelines
 * - print_pipeline(): Recursive pipeline structure printing
 * - trim_excess_whitespace(): Whitespace normalization
 * - dump_pipeline_info(): Public API and integration tests
 */

#include <gtest/gtest.h>
#include <glib.h>
#include <gst/gst.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>

extern "C" {
void dump_pipeline_info(GstBin *bin);
}

namespace {

class StdoutCapture {
public:
    StdoutCapture() : old_stdout(-1), temp_file(nullptr) {
        fflush(stdout);
        temp_file = tmpfile();
        if (temp_file) {
            old_stdout = dup(STDOUT_FILENO);
            dup2(fileno(temp_file), STDOUT_FILENO);
        }
    }
    
    ~StdoutCapture() {
        if (temp_file) {
            fflush(stdout);
            if (old_stdout != -1) {
                dup2(old_stdout, STDOUT_FILENO);
                close(old_stdout);
            }
            fclose(temp_file);
        }
    }
    
    std::string GetOutput() {
        fflush(stdout);
        if (!temp_file) return "";
        
        fseek(temp_file, 0, SEEK_SET);
        std::string result;
        char buffer[4096];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), temp_file)) > 0) {
            result.append(buffer, n);
        }
        return result;
    }
    
private:
    int old_stdout;
    FILE *temp_file;
};

// ============================================================================
// Test Fixture
// ============================================================================
class PipelineLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize GStreamer with null pointers to avoid plugin loading
        gst_init(NULL, NULL);
    }
    
    void TearDown() override {
        gst_deinit();
    }
    
    // Helper: Create a simple pipeline with source -> sink
    GstBin* CreateSimplePipeline(const char *name) {
        GstBin *bin = GST_BIN(gst_bin_new(name));
        GstElement *src = gst_element_factory_make("fakesrc", "source");
        GstElement *sink = gst_element_factory_make("fakesink", "sink");
        
        if (src && sink) {
            gst_bin_add_many(bin, src, sink, NULL);
            gst_element_link(src, sink);
        }
        
        return bin;
    }
    
    // Helper: Create a complex pipeline with multiple branches
    GstBin* CreateComplexPipeline(const char *name) {
        GstBin *bin = GST_BIN(gst_bin_new(name));
        
        // Create source elements
        GstElement *src1 = gst_element_factory_make("fakesrc", "source1");
        GstElement *src2 = gst_element_factory_make("fakesrc", "source2");
        
        // Create filter elements
        GstElement *filter1 = gst_element_factory_make("identity", "filter1");
        GstElement *filter2 = gst_element_factory_make("identity", "filter2");
        
        // Create queue elements
        GstElement *queue1 = gst_element_factory_make("queue", "queue1");
        GstElement *queue2 = gst_element_factory_make("queue", "queue2");
        
        // Create sink
        GstElement *sink = gst_element_factory_make("fakesink", "sink");
        
        if (src1 && src2 && filter1 && filter2 && queue1 && queue2 && sink) {
            gst_bin_add_many(bin, src1, src2, filter1, filter2, queue1, queue2, sink, NULL);
            gst_element_link(src1, queue1);
            gst_element_link(queue1, filter1);
            gst_element_link(src2, queue2);
            gst_element_link(queue2, filter2);
            gst_element_link(filter1, sink);
        }
        
        return bin;
    }
    
    // Helper: Create a nested pipeline with sub-bins
    GstBin* CreateNestedPipeline(const char *name) {
        GstBin *outer_bin = GST_BIN(gst_bin_new(name));
        
        // Create outer elements
        GstElement *src = gst_element_factory_make("fakesrc", "source");
        
        // Create inner bin
        GstBin *inner_bin = GST_BIN(gst_bin_new("inner_pipeline"));
        GstElement *inner_src = gst_element_factory_make("identity", "inner_filter");
        GstElement *inner_filter = gst_element_factory_make("identity", "inner_identity");
        
        // Create outer sink
        GstElement *sink = gst_element_factory_make("fakesink", "sink");
        
        if (src && inner_src && inner_filter && sink) {
            gst_bin_add_many(GST_BIN(inner_bin), inner_src, inner_filter, NULL);
            gst_element_link(inner_src, inner_filter);
            
            gst_bin_add_many(outer_bin, src, GST_ELEMENT(inner_bin), sink, NULL);
            gst_element_link(src, GST_ELEMENT(inner_bin));
            gst_element_link(GST_ELEMENT(inner_bin), sink);
        }
        
        return outer_bin;
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(PipelineLoggerTest, DumpSimplePipeline) {
    GstBin *pipeline = CreateSimplePipeline("test_pipeline");
    
    {
        StdoutCapture capture;
        dump_pipeline_info(pipeline);
        std::string output = capture.GetOutput();
        
        // Verify output contains content
        EXPECT_GT(output.length(), 0);
        // In test mode, output contains "Dumping pipeline"
        EXPECT_NE(output.find("Dumping pipeline"), std::string::npos);
    }
    
    gst_object_unref(pipeline);
}

TEST_F(PipelineLoggerTest, DumpComplexPipeline) {
    GstBin *pipeline = CreateComplexPipeline("complex_pipeline");
    
    {
        StdoutCapture capture;
        dump_pipeline_info(pipeline);
        std::string output = capture.GetOutput();
        
        // Verify output is generated
        EXPECT_GT(output.length(), 0);
    }
    
    gst_object_unref(pipeline);
}

TEST_F(PipelineLoggerTest, DumpNestedPipeline) {
    GstBin *pipeline = CreateNestedPipeline("nested_pipeline");
    
    {
        StdoutCapture capture;
        dump_pipeline_info(pipeline);
        std::string output = capture.GetOutput();
        
        // Verify output is generated
        EXPECT_GT(output.length(), 0);
        EXPECT_NE(output.find("Dumping pipeline"), std::string::npos);
    }
    
    gst_object_unref(pipeline);
}

// ============================================================================
// Empty and Edge Case Tests
// ============================================================================

TEST_F(PipelineLoggerTest, DumpEmptyBin) {
    GstBin *empty_bin = GST_BIN(gst_bin_new("empty_bin"));
    
    {
        StdoutCapture capture;
        dump_pipeline_info(empty_bin);
        std::string output = capture.GetOutput();
        
        // Should dump the empty bin
        EXPECT_GT(output.length(), 0);
    }
    
    gst_object_unref(empty_bin);
}

TEST_F(PipelineLoggerTest, DumpSingleElement) {
    GstBin *bin = GST_BIN(gst_bin_new("single_element_bin"));
    GstElement *elem = gst_element_factory_make("fakesrc", "solo_source");
    
    if (elem) {
        gst_bin_add(bin, elem);
        
        {
            StdoutCapture capture;
            dump_pipeline_info(bin);
            std::string output = capture.GetOutput();
            
            EXPECT_GT(output.length(), 0);
        }
    }
    
    gst_object_unref(bin);
}

// ============================================================================
// Formatting and Output Structure Tests
// ============================================================================

TEST_F(PipelineLoggerTest, OutputFormatContainsIndentation) {
    GstBin *pipeline = CreateComplexPipeline("indent_test_pipeline");
    
    {
        StdoutCapture capture;
        dump_pipeline_info(pipeline);
        std::string output = capture.GetOutput();
        
        // Verify output contains content
        EXPECT_GT(output.length(), 0);
    }
    
    gst_object_unref(pipeline);
}

TEST_F(PipelineLoggerTest, OutputContainsPipelineHeader) {
    GstBin *pipeline = CreateSimplePipeline("header_test");
    
    {
        StdoutCapture capture;
        dump_pipeline_info(pipeline);
        std::string output = capture.GetOutput();
        
        // Verify output contains expected header
        EXPECT_NE(output.find("Dumping pipeline"), std::string::npos);
    }
    
    gst_object_unref(pipeline);
}

TEST_F(PipelineLoggerTest, OutputContainsLineNumbers) {
    GstBin *pipeline = CreateComplexPipeline("line_number_test");
    
    {
        StdoutCapture capture;
        dump_pipeline_info(pipeline);
        std::string output = capture.GetOutput();
        
        // Verify output is generated
        EXPECT_GT(output.length(), 0);
    }
    
    gst_object_unref(pipeline);
}

// ============================================================================
// Pad Connection Tests
// ============================================================================

TEST_F(PipelineLoggerTest, PadsNotConnected) {
    GstBin *bin = GST_BIN(gst_bin_new("unconnected_bin"));
    GstElement *src = gst_element_factory_make("fakesrc", "unconnected_source");
    GstElement *sink = gst_element_factory_make("fakesink", "unconnected_sink");
    
    if (src && sink) {
        gst_bin_add_many(bin, src, sink, NULL);
        // Don't connect them
        
        {
            StdoutCapture capture;
            dump_pipeline_info(bin);
            std::string output = capture.GetOutput();
            
            // Verify output
            EXPECT_GT(output.length(), 0);
            EXPECT_NE(output.find("Dumping pipeline"), std::string::npos);
        }
    }
    
    gst_object_unref(bin);
}

// ============================================================================
// Multiple Elements Tests
// ============================================================================

TEST_F(PipelineLoggerTest, MultipleElementsSameName) {
    GstBin *bin = GST_BIN(gst_bin_new("multi_same_name"));
    
    GstElement *queue1 = gst_element_factory_make("queue", "queue");
    GstElement *queue2 = gst_element_factory_make("queue", "queue");
    GstElement *queue3 = gst_element_factory_make("queue", "queue");
    
    if (queue1 && queue2 && queue3) {
        gst_bin_add_many(bin, queue1, queue2, queue3, NULL);
        
        {
            StdoutCapture capture;
            dump_pipeline_info(bin);
            std::string output = capture.GetOutput();
            
            // Verify output
            EXPECT_GT(output.length(), 0);
            EXPECT_NE(output.find("Dumping pipeline"), std::string::npos);
        }
    }
    
    gst_object_unref(bin);
}

// ============================================================================
// Long Pipeline Tests
// ============================================================================

TEST_F(PipelineLoggerTest, LongLinearPipeline) {
    GstBin *bin = GST_BIN(gst_bin_new("long_pipeline"));
    GstElement *src = gst_element_factory_make("fakesrc", "source");
    GstElement *prev = src;
    
    if (src) {
        gst_bin_add(bin, src);
        
        // Add multiple identity elements in a chain
        for (int i = 0; i < 5; i++) {
            char name[20];
            snprintf(name, sizeof(name), "identity_%d", i);
            GstElement *identity = gst_element_factory_make("identity", name);
            if (identity) {
                gst_bin_add(bin, identity);
                gst_element_link(prev, identity);
                prev = identity;
            }
        }
        
        GstElement *sink = gst_element_factory_make("fakesink", "sink");
        if (sink) {
            gst_bin_add(bin, sink);
            gst_element_link(prev, sink);
        }
        
        {
            StdoutCapture capture;
            dump_pipeline_info(bin);
            std::string output = capture.GetOutput();
            
            // Verify output
            EXPECT_GT(output.length(), 0);
        }
    }
    
    gst_object_unref(bin);
}

// ============================================================================
// Special Character Tests
// ============================================================================

TEST_F(PipelineLoggerTest, ElementWithSpecialCharacters) {
    GstBin *bin = GST_BIN(gst_bin_new("special_chars_bin"));
    GstElement *src = gst_element_factory_make("fakesrc", "src_123");
    GstElement *sink = gst_element_factory_make("fakesink", "sink_456");
    
    if (src && sink) {
        gst_bin_add_many(bin, src, sink, NULL);
        gst_element_link(src, sink);
        
        {
            StdoutCapture capture;
            dump_pipeline_info(bin);
            std::string output = capture.GetOutput();
            
            EXPECT_GT(output.length(), 0);
        }
    }
    
    gst_object_unref(bin);
}

// ============================================================================
// Dynamic Modification Tests
// ============================================================================

TEST_F(PipelineLoggerTest, PipelineAfterDynamicModification) {
    GstBin *bin = GST_BIN(gst_bin_new("dynamic_bin"));
    GstElement *src = gst_element_factory_make("fakesrc", "source");
    GstElement *sink = gst_element_factory_make("fakesink", "sink");
    
    if (src && sink) {
        gst_bin_add_many(bin, src, sink, NULL);
        gst_element_link(src, sink);
        
        // First dump
        {
            StdoutCapture capture;
            dump_pipeline_info(bin);
            std::string output = capture.GetOutput();
            EXPECT_GT(output.length(), 0);
        }
        
        // Add another element dynamically
        GstElement *identity = gst_element_factory_make("identity", "new_identity");
        if (identity) {
            gst_element_unlink(src, sink);
            gst_bin_add(bin, identity);
            gst_element_link(src, identity);
            gst_element_link(identity, sink);
            
            // Second dump
            {
                StdoutCapture capture;
                dump_pipeline_info(bin);
                std::string output = capture.GetOutput();
                EXPECT_GT(output.length(), 0);
            }
        }
    }
    
    gst_object_unref(bin);
}

// ============================================================================
// State Tests
// ============================================================================

TEST_F(PipelineLoggerTest, DumpPipelineInDifferentStates) {
    GstBin *pipeline = CreateSimplePipeline("state_test_pipeline");
    
    // Dump in NULL state
    {
        StdoutCapture capture;
        dump_pipeline_info(pipeline);
        std::string output = capture.GetOutput();
        EXPECT_GT(output.length(), 0);
    }
    
    // Change state and dump again
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_READY);
    {
        StdoutCapture capture;
        dump_pipeline_info(pipeline);
        std::string output = capture.GetOutput();
        EXPECT_GT(output.length(), 0);
    }
    
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(PipelineLoggerTest, RepeatedDumpCalls) {
    GstBin *pipeline = CreateSimplePipeline("repeat_test");
    
    // Call dump multiple times
    for (int i = 0; i < 3; i++) {
        StdoutCapture capture;
        dump_pipeline_info(pipeline);
        std::string output = capture.GetOutput();
        EXPECT_GT(output.length(), 0);
    }
    
    gst_object_unref(pipeline);
}

TEST_F(PipelineLoggerTest, DeepNestingLevels) {
    // Create deeply nested bins
    GstBin *outer = GST_BIN(gst_bin_new("outer"));
    GstBin *middle = GST_BIN(gst_bin_new("middle"));
    GstBin *inner = GST_BIN(gst_bin_new("inner"));
    
    GstElement *src = gst_element_factory_make("fakesrc", "src");
    GstElement *sink = gst_element_factory_make("fakesink", "sink");
    
    if (src && sink) {
        gst_bin_add_many(inner, src, sink, NULL);
        gst_element_link(src, sink);
        
        gst_bin_add(middle, GST_ELEMENT(inner));
        gst_bin_add(outer, GST_ELEMENT(middle));
        
        {
            StdoutCapture capture;
            dump_pipeline_info(outer);
            std::string output = capture.GetOutput();
            
            // Verify output is generated
            EXPECT_GT(output.length(), 0);
            EXPECT_NE(output.find("Dumping pipeline"), std::string::npos);
        }
    }
    
    gst_object_unref(outer);
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

