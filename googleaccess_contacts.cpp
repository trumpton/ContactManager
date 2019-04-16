
//
// Some important notes about the Google Contacts People API:
//
// Updates are not immediately reflected in contacts when 'getcontact' is used
// This means that:
//
//    Update contact returns a Json response with the expected changes
//    getContact shows the old status
//    some time later, a repeated getContact correctly reflets the changes
//
// So, don't use getContact to verify that the changes have been embodied
//
// In order to add/remove users from groups, a different API is used, and in a similar
// way, it takes time for these updates to be reflected in getContact
//
// BUT, there is a bug in the Google group management process, in that if two groups are
// modified for the same user (e.g. remove him from one, and add him to the other), the
// group management API returns success, but only one of the changes are actually implemented.
//
// This means that the only practical way to reliably modify group membership at the moment is
// to do one group at a time, and wait for the changes to propagate:
//
//   Modify group membership
//   Loop
//    Get Contact
//   Until contact appears in correct group, or some timout occurs
//

//
// Format of Contact
//
//
// "resourceName": "people/xxxxxxxxxxxxxxxxxxxx",
// "etag": "...........................................................",
// "metadata": { "deleted": true },
// "names": [{
//    "metadata": { "source": { "type": "CONTACT" }},
//    "familyName": "SURNAME",
//    "givenName": "FIRSTNAME",
//    "middleName": "MIDDLENAME"
// }],
// "birthdays": [{
//    "metadata": { "source": { "type": "CONTACT" }},
//    "date": {
//        "year": YYYY,
//        "month": MM,
//        "day": DD
//    }
// }],
// "addresses": [{
//    "metadata": { "source": { "type": "CONTACT" }},
//    "type": "home/work/other",
//    "formattedValue": "house street, pobox, street line 2, town, POSTCODE, Country"
// }],
// "emailAddresses": [{
//    "metadata": { "source": { "type": "CONTACT" }},
//     "value": "EMAILADDRESS",
//     "type": "home/work/other",
// }],
// "phoneNumbers": [{
//    "metadata": { "source": { "type": "CONTACT" }},
//     "value": "PHONENUMBERWITHSPACES",
//     "canonicalForm": "PHONENUMBER",
//     "type": "TYPE"
// }],
// "biographies": [{
//    "metadata": { "source": { "type": "CONTACT" }},
//      "value": "NOTES",
//      "contentType": "TEXT_PLAIN"
// }],
// "memberships": [{
//    "metadata": { "source": { "type": "CONTACT" }},
//        "contactGroupMembership": {
//          "contactGroupId": "CONTACTGROUP"
//        }
//    },{
//        "contactGroupMembership": {
//          "contactGroupId": "myContacts"
//    }
// }],
// "birthdays": [{
//    "metadata": { "source": { "type": "CONTACT" }},
//    "date": {
//        "year": YYYY,
//        "month": MM,
//        "day": DD
//    }
// }],
// "organizations": [{
//    "metadata": { "source": { "type": "CONTACT" }},
//    "type": "Work",
//    "name": "ORGANISATION"
// }],
// "userDefined": [{
//       "key": "CUSTOMLABEL1",
//       "value": "CUSTOM1"
// },{
//       "key": "CUSTOMLABEL2",
//       "value": "CUSTOM2"
// }]
//
