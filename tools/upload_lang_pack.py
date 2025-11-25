# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import logging
import os
import pbpack
import polib
import requests
import sys
import tempfile


logging.basicConfig()
logging.getLogger().setLevel(logging.DEBUG)
requests_log = logging.getLogger("requests.packages.urllib3")
requests_log.setLevel(logging.DEBUG)
requests_log.propagate = True


LP_URLS = {"staging": "https://lang-packs-staging.getpebble.com",
           "production": "https://lp.getpebble.com"}
lp_url = None

if 'LP_HTTP_USERNAME' not in os.environ or 'LP_HTTP_PASSWORD' not in os.environ:
    print """Missing HTTP username/password!
Please define LP_HTTP_USERNAME and LP_HTTP_PASSWORD in your environment"""
    sys.exit(1)

lp_http_username = os.environ['LP_HTTP_USERNAME']
lp_http_password = os.environ['LP_HTTP_PASSWORD']
lp_auth = requests.auth.HTTPBasicAuth(lp_http_username, lp_http_password)

FIRMWARE_VERSION_TO_UPDATE = 4


def assert_ok(response):
    if response.status_code != requests.codes.ok:
        print response.status_code
        print response.text


def find_existing_pack(hardware, lang):
    # Look for an existing language pack. Use X.99.99 as the requesting version as the
    # lp server will only return packs that are for the same major version and a smaller
    # minor version, and we want the most recent pack with the highest minor version there is.
    r = requests.get('{}/v1/languages'.format(lp_url),
                     params={'hardware': hardware,
                             'firmware': '{}.99.99'.format(FIRMWARE_VERSION_TO_UPDATE)},
                     auth=lp_auth)
    assert_ok(r)

    for l in r.json()['languages']:
        if l['ISOLocal'] == lang:
            return l

    return None


def create_new_pack_json(hardware, name, fw_version, metadata):
    return {
             'name': name,
             'localName': metadata['Name'],
             'hardware': hardware,
             'ISOLocal': metadata['Language'],
             'firmware': fw_version,
             'mobile': {
               'name': 'ios',
               'version': '2.6.0'
             }
           }


def pack_json_to_post_data(j):
    # The POST data that the form wants looks like the following...
    #
    # pack[file]:https://language-packs-staging.s3.amazonaws.com/3TRBOAT-en_US.pbl
    # pack[name]:English
    # pack[localName]:English
    # pack[hardware]:bb2
    # pack[ISOLocal]:en_US
    # pack[version]:1
    # pack[firmware]:2.9.0
    # pack[mobile][name]:ios
    # pack[mobile][version]:2.6.0
    #
    # Convert our json into that form

    fields = ('file', 'name', 'localName', 'hardware', 'ISOLocal', 'version', 'firmware')
    d = {'pack[{}]'.format(f): j[f] for f in fields}

    d['pack[mobile][name]'] = j['mobile']['name']
    d['pack[mobile][version]'] = j['mobile']['version']

    return d


def get_lang_metadata(lang_pack):
    mo_filename = None

    # Open up the lang_pack as a pbpack. The first entry is the STRINGS values, which is a mo
    # file. Write this mo file out to a temporary file and use polib to parse out the metadata.
    #
    # The resulting dictionary will look something like the following...
    #
    # {u'Content-Transfer-Encoding': u'8bit',
    #  u'Content-Type': u'text/plain; charset=UTF-8',
    #  u'Language': u'fr_FR',
    #  u'MIME-Version': u'1.0',
    #  u'Name': u'Fran\xe7ais',
    #  u'POT-Creation-Date': u'2016-06-28 15:24-0400',
    #  u'Project-Id-Version': u'33.0',
    #  u'Report-Msgid-Bugs-To': u'',
    #  u'X-Generator': u'POEditor.com'}
    try:
        with open(lang_pack, 'rb') as f:
            pack = pbpack.ResourcePack.deserialize(f, is_system=False)
            with tempfile.NamedTemporaryFile(delete=False) as mo_file:
                mo_filename = mo_file.name

                # RESOURCE_ID_STRINGS is the 0th resource in the pb pack. Grab the data and write
                # it to a file.
                content_index = pack.table_entries[0].content_index
                if len(pack.contents[content_index]) == 0:
                    # Assume this is an english pack
                    return {u'Language': u'en_US',
                            u'Name': u'English',
                            u'Project-Id-Version': u'1.0'}

                mo_file.write(pack.contents[content_index])

        mo_object = polib.mofile(mo_filename)
    finally:
        os.remove(mo_filename)

    metadata = mo_object.metadata

    # Validate the version field just to be sure
    assert 'Project-Id-Version' in metadata
    version_parts = metadata['Project-Id-Version'].split('.')
    assert len(version_parts) == 2
    int(version_parts[0])  # Just make sure it can be converted to a valid integer

    return metadata


def upload_pack_to_s3(lang_pack):
    r = requests.get('{}/admin/packs/sign'.format(lp_url),
                     params={'type': '', 'name': os.path.basename(lang_pack)},
                     auth=lp_auth)
    assert_ok(r)

    s3_upload_info = r.json()

    with open(lang_pack, 'rb') as f:
        r = requests.put(s3_upload_info['signed'], f)
        assert_ok(r)

    return s3_upload_info['url']


def post_edit_to_lp(pack_json):
    post_data = pack_json_to_post_data(pack_json)

    r = requests.post('{}/admin/packs/{}/edit'.format(lp_url, pack_json['id']),
                      data=post_data,
                      auth=lp_auth)
    assert_ok(r)


def create_new_pack_on_lp(pack_json):
    post_data = pack_json_to_post_data(pack_json)

    r = requests.post('{}/admin/packs/add'.format(lp_url),
                      data=post_data,
                      auth=lp_auth)
    assert_ok(r)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('hardware')
    parser.add_argument('lang_pack')
    parser.add_argument('--production', action='store_true')
    parser.add_argument('--allow_new', action='store_true')
    parser.add_argument('--name', default=None)
    parser.add_argument('--fw_version', default='4.0.0')

    args = parser.parse_args()

    global lp_url
    if args.production:
        lp_url = LP_URLS["production"]
    else:
        lp_url = LP_URLS["staging"]

    metadata = get_lang_metadata(args.lang_pack)

    is_new = False

    pack_json = find_existing_pack(args.hardware, metadata['Language'])

    if pack_json is None:
        if not args.allow_new:
            print "No existing pack found"
            return 1

        if args.name is None:
            print "Name is required for new packs"
            return 1

        pack_json = create_new_pack_json(args.hardware, args.name, args.fw_version, metadata)
        is_new = True

    # Update the pack with the new information
    pack_json['version'] = metadata['Project-Id-Version'].split('.')[0]
    pack_json['file'] = upload_pack_to_s3(args.lang_pack)

    if is_new:
        create_new_pack_on_lp(pack_json)
    else:
        post_edit_to_lp(pack_json)


if __name__ == '__main__':
    main()
