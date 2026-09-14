// goface Clay config page. Values are sent to the watch via AppMessage.
module.exports = [
  {
    "type": "heading",
    "defaultValue": "goface Settings"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Problem Set"
      },
      {
        "type": "select",
        "messageKey": "ProblemSet",
        "defaultValue": "0",
        "label": "Difficulty",
        "options": [
          { "label": "Random", "value": "0" },
          { "label": "Easy", "value": "1" },
          { "label": "Intermediate", "value": "2" },
          { "label": "Hard", "value": "3" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Timing (seconds)"
      },
      {
        "type": "input",
        "messageKey": "ResetSeconds",
        "defaultValue": "10",
        "label": "Board reset idle (s)",
        "attributes": { "type": "number", "min": "0", "max": "300" }
      },
      {
        "type": "input",
        "messageKey": "NewProblemSeconds",
        "defaultValue": "60",
        "label": "New problem idle (s)",
        "attributes": { "type": "number", "min": "0", "max": "3600" }
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Clock"
      },
      {
        "type": "select",
        "messageKey": "ClockFormat",
        "defaultValue": "12",
        "label": "Format",
        "options": [
          { "label": "12-hour (AM/PM)", "value": "12" },
          { "label": "24-hour", "value": "24" }
        ]
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
