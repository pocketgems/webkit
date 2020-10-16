#! /usr/bin/env python2.7

from __future__ import print_function

import os
import sys

def setup_path():
    env = os.environ
    if not 'MANTIS_TOOLCHAIN_DIR' in env:
        print('MANTIS_TOOLCHAIN_DIR must be set', file=sys.stderr)
        sys.exit(1)
    mantis_toolchain_dir = env['MANTIS_TOOLCHAIN_DIR']
    sys.path.append(os.path.join(mantis_toolchain_dir, 'google_drive'))

def mantis_toolchain_drive_id():
    return '0AKVeIdySA6OhUk9PVA'

def main():
    setup_path()
    filepath = sys.argv[1]
    from drive_package_manager import create_service, upload
    service = create_service()
    upload(service, filepath, os.path.basename(filepath), 'application/gzip', mantis_toolchain_drive_id())


if __name__ == '__main__':
    main()
