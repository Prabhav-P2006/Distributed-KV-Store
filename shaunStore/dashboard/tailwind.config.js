/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        slatebg: "#07111f",
        ink: "#dbe7ff",
        grid: "#13304e",
        critical: "#ff5d5d",
        standard: "#49c7ff",
        low: "#61d67c",
        aging: "#ffae42",
        accent: "#8cf4ff",
        gold: "#ffd369",
      },
      boxShadow: {
        neon: "0 0 0 1px rgba(140,244,255,0.18), 0 12px 40px rgba(0, 0, 0, 0.45)",
      },
    },
  },
  plugins: [],
}
