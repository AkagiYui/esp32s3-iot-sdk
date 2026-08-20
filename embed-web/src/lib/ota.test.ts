import { describe, expect, it } from "vite-plus/test";
import { validateFirmwareFile } from "./ota";

function makeFile(name: string, size: number): File {
  const file = new File([""], name);
  Object.defineProperty(file, "size", { value: size });
  return file;
}

describe("validateFirmwareFile", () => {
  const maxSize = 3 * 1024 * 1024;

  it("接受合法固件", () => {
    expect(validateFirmwareFile(makeFile("app.bin", 1024), maxSize)).toBeUndefined();
  });

  it("拒绝空文件", () => {
    expect(validateFirmwareFile(makeFile("app.bin", 0), maxSize)).toBe("文件为空");
  });

  it("拒绝超出 OTA 分区容量的固件", () => {
    expect(validateFirmwareFile(makeFile("app.bin", maxSize + 1), maxSize)).toBe(
      "固件超出 OTA 分区容量",
    );
  });

  it("设备未上报分区容量时不做体积限制", () => {
    expect(validateFirmwareFile(makeFile("app.bin", 99_999_999), 0)).toBeUndefined();
  });

  it("拒绝非 .bin 文件", () => {
    expect(validateFirmwareFile(makeFile("app.elf", 1024), maxSize)).toBe("请选择 .bin 固件文件");
  });
});
