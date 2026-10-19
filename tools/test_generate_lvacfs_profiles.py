#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0

import sys
sys.dont_write_bytecode = True

from pathlib import Path
import tempfile
import unittest

from generate_lvacfs_profiles import generate


class GenerateProfilesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def configuration(self, group, text):
        directory = self.root / group
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / "LVACFS_Configuration.txt"
        path.write_text(text)
        return path

    def test_table_id_overrides_filename_id(self):
        path = self.configuration("2mic", "1 // DeviceId1\n"
                                  "/vendor/LVIMFS_Parameter_xxxx_ID5_MIC_Normal.txt // Profile\n")
        before = path.read_bytes()
        header = generate(self.root)
        self.assertIn("MIC_NORMAL = 1,", header)
        self.assertNotIn("MIC_NORMAL = 5,", header)
        self.assertEqual(path.read_bytes(), before)

    def test_same_file_can_have_multiple_ids(self):
        self.configuration("2mic", "1\n/vendor/LVACFS_Parameter_ID1_MIC_Normal.txt\n"
                           "0\n/vendor/LVACFS_Parameter_ID1_MIC_Normal.txt\n")
        header = generate(self.root)
        self.assertIn("MIC_NORMAL = 1,", header)
        self.assertIn("MIC_NORMAL_ID0 = 0,", header)

    def test_configuration_groups_are_separate(self):
        for group, value in [("1mic", 5), ("2mic", 7)]:
            self.configuration(group, f"{value}\n/vendor/LVACFS_Parameter_MIC_Normal.txt\n")
        header = generate(self.root)
        self.assertIn("namespace lvacfs::profiles::config_1mic", header)
        self.assertIn("namespace lvacfs::profiles::config_2mic", header)
        self.assertIn("MIC_NORMAL = 5,", header)
        self.assertIn("MIC_NORMAL = 7,", header)

    def test_duplicate_ids_are_rejected(self):
        self.configuration("2mic", "5\n/vendor/LVACFS_Parameter_MIC_Normal.txt\n"
                           "5\n/vendor/LVACFS_Parameter_MIC_App.txt\n")
        with self.assertRaisesRegex(ValueError, "duplicate profile ID"):
            generate(self.root)

    def test_empty_directory_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "no LVACFS_Configuration.txt"):
            generate(self.root)

    def test_invalid_table_is_rejected(self):
        self.configuration("2mic", "5\n/vendor/calibration.dat\n")
        with self.assertRaisesRegex(ValueError, "no profile entries"):
            generate(self.root)

    def test_output_is_deterministic(self):
        self.configuration("2mic", "5\n/vendor/LVACFS_Parameter_MIC_Normal.txt\n")
        self.assertEqual(generate(self.root), generate(self.root))


if __name__ == "__main__":
    unittest.main()
