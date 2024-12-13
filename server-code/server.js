const express = require('express');
const path = require('path');

// Creating an Express application
const app = express();

// Serve the static HTML file (speed_limit.html)
app.use(express.static(path.join(__dirname, 'public')));

// Start the server on port 5000
const PORT = 3000;
app.listen(PORT, () => { console.log(`Server running on port ${PORT}`); });

